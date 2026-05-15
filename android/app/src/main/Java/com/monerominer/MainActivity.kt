package com.monerominer

import android.annotation.SuppressLint
import android.os.Bundle
import android.util.Log
import android.webkit.JavascriptInterface
import android.webkit.WebChromeClient
import android.webkit.WebView
import android.webkit.WebViewClient
import android.widget.Toast
import androidx.appcompat.app.AppCompatActivity

class MainActivity : AppCompatActivity() {
    
    private lateinit var webView: WebView
    private var nativeLibraryLoaded = false
    
    companion object {
        private const val TAG = "MainActivity"
    }
    
    @SuppressLint("SetJavaScriptEnabled")
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        
        Log.d(TAG, "onCreate: Initializing MainActivity")
        
        // Load native library
        loadNativeLibrary()
        
        // Initialize WebView
        webView = WebView(this).apply {
            settings.apply {
                javaScriptEnabled = true
                domStorageEnabled = true
                allowFileAccess = true
                allowContentAccess = true
                mixedContentMode = WebViewClient.MIXED_CONTENT_ALWAYS_ALLOW
            }
            webViewClient = object : WebViewClient() {
                override fun onPageFinished(view: WebView?, url: String?) {
                    super.onPageFinished(view, url)
                    Log.d(TAG, "WebView page finished loading: $url")
                }
                
                override fun onReceivedError(
                    view: WebView?,
                    errorCode: Int,
                    description: String?,
                    failingUrl: String?
                ) {
                    super.onReceivedError(view, errorCode, description, failingUrl)
                    Log.e(TAG, "WebView error: $errorCode - $description at $failingUrl")
                    webView.loadDataWithBaseURL(
                        null,
                        "<html><body><h2>Error Loading Page</h2><p>$description</p></body></html>",
                        "text/html",
                        "utf-8",
                        null
                    )
                }
            }
            webChromeClient = WebChromeClient()
            addJavascriptInterface(MinerBridge(), "MinerBridge")
            
            // Load the HTML UI
            try {
                loadUrl("file:///android_asset/index.html")
                Log.d(TAG, "Loading index.html from assets")
            } catch (e: Exception) {
                Log.e(TAG, "Failed to load index.html", e)
                loadDataWithBaseURL(
                    null,
                    "<html><body><h2>Error</h2><p>Failed to load mining interface</p></body></html>",
                    "text/html",
                    "utf-8",
                    null
                )
            }
        }
        
        setContentView(webView)
        Log.d(TAG, "MainActivity initialization complete")
    }
    
    private fun loadNativeLibrary() {
        try {
            System.loadLibrary("monerominer")
            nativeLibraryLoaded = true
            Log.d(TAG, "Native library 'monerominer' loaded successfully")
        } catch (e: UnsatisfiedLinkError) {
            Log.e(TAG, "Failed to load native library: ${e.message}")
            nativeLibraryLoaded = false
            Toast.makeText(
                this,
                "Warning: Mining library failed to load. Some features may not work.",
                Toast.LENGTH_LONG
            ).show()
        } catch (e: Exception) {
            Log.e(TAG, "Error loading native library: ${e.message}")
            nativeLibraryLoaded = false
        }
    }
    
    override fun onBackPressed() {
        if (webView.canGoBack()) {
            webView.goBack()
        } else {
            super.onBackPressed()
        }
    }
    
    override fun onResume() {
        super.onResume()
        webView.onResume()
        Log.d(TAG, "onResume")
    }
    
    override fun onPause() {
        webView.onPause()
        super.onPause()
        Log.d(TAG, "onPause")
    }
    
    override fun onDestroy() {
        webView.destroy()
        super.onDestroy()
        Log.d(TAG, "onDestroy")
    }
    
    /**
     * JavaScript Interface Bridge for WebView
     * Exposes native mining functions to JavaScript
     */
    inner class MinerBridge {
        @JavascriptInterface
        fun startMining(host: String, port: Int, wallet: String, worker: String, threads: Int): Boolean {
            return try {
                if (!nativeLibraryLoaded) {
                    Log.e(TAG, "Native library not loaded")
                    return false
                }
                Log.d(TAG, "startMining: host=$host, port=$port, wallet=$wallet, threads=$threads")
                nativeStartMining(host, port, wallet, worker, threads)
            } catch (e: Exception) {
                Log.e(TAG, "Error in startMining: ${e.message}", e)
                false
            }
        }
        
        @JavascriptInterface
        fun stopMining() {
            try {
                if (nativeLibraryLoaded) {
                    Log.d(TAG, "stopMining called")
                    nativeStopMining()
                }
            } catch (e: Exception) {
                Log.e(TAG, "Error in stopMining: ${e.message}", e)
            }
        }
        
        @JavascriptInterface
        fun getStats(): String {
            return try {
                if (nativeLibraryLoaded) {
                    nativeGetStats()
                } else {
                    "{\"status\": \"not_loaded\", \"hashrate\": 0}"
                }
            } catch (e: Exception) {
                Log.e(TAG, "Error in getStats: ${e.message}", e)
                "{\"status\": \"error\", \"hashrate\": 0}"
            }
        }
        
        @JavascriptInterface
        fun isNativeLoaded(): Boolean = nativeLibraryLoaded
    }
    
    // Native JNI functions
    private external fun nativeStartMining(
        host: String,
        port: Int,
        wallet: String,
        worker: String,
        threads: Int
    ): Boolean
    
    private external fun nativeStopMining()
    private external fun nativeGetStats(): String
}
