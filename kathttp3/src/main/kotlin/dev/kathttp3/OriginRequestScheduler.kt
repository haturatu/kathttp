package dev.kathttp3

import kotlinx.coroutines.CancellableContinuation
import kotlinx.coroutines.TimeoutCancellationException
import kotlinx.coroutines.suspendCancellableCoroutine
import kotlinx.coroutines.withTimeout
import java.net.URI
import java.util.ArrayDeque
import java.util.Locale
import java.util.concurrent.atomic.AtomicBoolean

/**
 * Client-side admission control, deliberately independent of QUIC's peer
 * MAX_STREAMS transport parameter. A permit is held from native submission to
 * the terminal callback, so queued calls have not created native request or
 * stream state yet.
 */
internal class OriginRequestScheduler(
    private val maxActive: Int,
    private val maxQueued: Int,
    private val queueTimeoutMillis: Long,
) : AutoCloseable {
    private data class Waiter(
        val continuation: CancellableContinuation<Permit>,
    )

    private data class OriginState(
        var active: Int = 0,
        val waiters: Array<ArrayDeque<Waiter>> = Array(8) { ArrayDeque() },
    )

    private val lock = Any()
    private val origins = mutableMapOf<String, OriginState>()
    private var closed = false

    suspend fun acquire(
        url: String,
        priority: KatHttp3RequestPriority = KatHttp3RequestPriority(),
    ): Permit {
        val origin = originOf(url)
        try {
            return withTimeout(queueTimeoutMillis) {
                suspendCancellableCoroutine { continuation ->
                    var acquired = false
                    synchronized(lock) {
                        if (closed) {
                            continuation.resumeWith(Result.failure(KatHttp3Exception.Closed()))
                        } else {
                            val state = origins.getOrPut(origin) { OriginState() }
                            if (state.active < maxActive) {
                                state.active += 1
                                acquired = true
                            } else if (state.waiters.sumOf { it.size } >= maxQueued) {
                                continuation.resumeWith(
                                    Result.failure(KatHttp3Exception.RequestQueueFull(origin)),
                                )
                            } else {
                                state.waiters[priority.urgency].addLast(Waiter(continuation))
                            }
                        }
                    }
                    if (acquired) {
                        continuation.resume(Permit(this@OriginRequestScheduler, origin)) {
                                _, permit, _ -> permit.close()
                        }
                    }
                    continuation.invokeOnCancellation { removeWaiter(origin, continuation) }
                }
            }
        } catch (_: TimeoutCancellationException) {
            throw KatHttp3Exception.RequestQueueTimeout(origin, queueTimeoutMillis)
        }
    }

    private fun removeWaiter(origin: String, continuation: CancellableContinuation<Permit>) {
        synchronized(lock) {
            val state = origins[origin] ?: return
            state.waiters.forEach { queue -> queue.removeAll { it.continuation == continuation } }
            removeIdleState(origin, state)
        }
    }

    private fun release(origin: String) {
        var next: CancellableContinuation<Permit>? = null
        synchronized(lock) {
            val state = origins[origin] ?: return
            for (queue in state.waiters) {
                while (queue.isNotEmpty()) {
                    val candidate = queue.removeFirst().continuation
                    if (candidate.isActive) {
                        next = candidate
                        break
                    }
                }
                if (next != null) break
            }
            if (next == null) {
                state.active -= 1
                removeIdleState(origin, state)
            }
        }
        next?.resume(Permit(this, origin)) { _, permit, _ -> permit.close() }
    }

    private fun removeIdleState(origin: String, state: OriginState) {
        if (state.active == 0 && state.waiters.all { it.isEmpty() }) origins.remove(origin)
    }

    override fun close() {
        val waiters = mutableListOf<CancellableContinuation<Permit>>()
        synchronized(lock) {
            if (closed) return
            closed = true
            origins.values.forEach { state ->
                state.waiters.forEach { queue ->
                    while (queue.isNotEmpty()) waiters += queue.removeFirst().continuation
                }
            }
            origins.clear()
        }
        waiters.forEach { continuation ->
            if (continuation.isActive) continuation.resumeWith(Result.failure(KatHttp3Exception.Closed()))
        }
    }

    class Permit internal constructor(
        private val scheduler: OriginRequestScheduler,
        private val origin: String,
    ) : AutoCloseable {
        private val released = AtomicBoolean(false)
        override fun close() {
            if (released.compareAndSet(false, true)) scheduler.release(origin)
        }
    }

    companion object {
        internal fun originOf(url: String): String {
            val uri = URI(url)
            val scheme = uri.scheme.lowercase(Locale.ROOT)
            val host = requireNotNull(uri.host) { "Request URL must have a host" }.lowercase(Locale.ROOT)
            val port = if (uri.port >= 0) uri.port else if (scheme == "https") 443 else -1
            return "$scheme://$host:$port"
        }
    }
}
