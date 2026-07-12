package dev.kathttp.example

import androidx.lifecycle.ViewModel
import androidx.lifecycle.viewModelScope
import dev.kathttp.*
import kotlinx.coroutines.Job
import kotlinx.coroutines.CancellationException
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.asStateFlow
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import kotlin.time.TimeSource

data class UiState(val url: String = "https://", val method: String = "GET", val loading: Boolean = false, val status: Int? = null, val headers: String = "", val body: String = "", val error: String? = null, val durationMs: Long? = null, val size: Int = 0, val protocol: String = "")

class MainViewModel : ViewModel() {
    private val client = KatHttpClient()
    private val mutable = MutableStateFlow(UiState())
    val state = mutable.asStateFlow()
    private var job: Job? = null
    fun setUrl(value: String) { mutable.value = mutable.value.copy(url = value) }
    fun setMethod(value: String) { mutable.value = mutable.value.copy(method = value) }
    fun execute(method: String = mutable.value.method) {
        job?.cancel(); val url = mutable.value.url; mutable.value = mutable.value.copy(method = method, loading = true, error = null)
        job = viewModelScope.launch {
            val mark = TimeSource.Monotonic.markNow()
            try {
                val response = client.execute(KatHttpRequest(method, url, if (method == "POST") listOf(KatHttpHeader("content-type", "application/json")) else emptyList(), if (method == "POST") "{\"hello\":\"h3\"}".encodeToByteArray() else null))
                val preview = withContext(Dispatchers.Default) { response.body.decodeToString().take(64_000) }
                mutable.value = UiState(url, method, false, response.status, response.headers.joinToString("\n") { "${it.name}: ${it.value}" }, preview, null, mark.elapsedNow().inWholeMilliseconds, response.body.size, response.protocol)
            } catch (cancelled: CancellationException) {
                throw cancelled
            } catch (error: Throwable) {
                mutable.value = mutable.value.copy(loading = false, error = error.message, durationMs = mark.elapsedNow().inWholeMilliseconds)
            }
        }
    }
    fun cancel() { job?.cancel(); job = null; mutable.value = mutable.value.copy(loading = false, error = "Cancelled") }
    override fun onCleared() { cancel(); client.close() }
}
