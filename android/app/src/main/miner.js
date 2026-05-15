// Monero Miner - JavaScript Controller
// Uses in-memory storage (works in Android WebView with or without DOM storage)
var mining = false;
var statsTimer = null;
var startTime = null;
var totalShares = {accepted: 0, rejected: 0};

// In-memory config store (fallback if localStorage unavailable)
var configStore = {};

function saveConfig(key, val) {
    try { localStorage.setItem(key, val); } catch(e) {}
    configStore[key] = val;
}
function loadConfig(key) {
    try {
        var v = localStorage.getItem(key);
        if (v !== null) return v;
    } catch(e) {}
    return configStore[key] || null;
}

function $(id){ return document.getElementById(id); }

function log(msg, type){
    var cls = 'log-info';
    if(type === 'warn') cls = 'log-warn';
    else if(type === 'error') cls = 'log-error';
    else if(type === 'share') cls = 'log-share';
    
    var t = new Date().toLocaleTimeString();
    var line = '<div class="log-line"><span class="log-time">[' + t + ']</span> <span class="' + cls + '">' + msg + '</span></div>';
    $('logContainer').innerHTML += line;
    $('logContainer').scrollTop = $('logContainer').scrollHeight;
}

function formatHashrate(hps) {
    if (hps >= 1000000) return (hps / 1000000).toFixed(2) + ' MH/s';
    if (hps >= 1000) return (hps / 1000).toFixed(2) + ' KH/s';
    return Math.round(hps) + ' H/s';
}

function updateStats(){
    if(!mining) return;
    
    if(window.MinerBridge && window.MinerBridge.getStats){
        try {
            var raw = window.MinerBridge.getStats();
            var stats = JSON.parse(raw);
            
            var hr = stats.hashrate || 0;
            $('hashrateVal').textContent = formatHashrate(hr);
            
            // Use native share counts if available and non-zero, else keep JS counts
            if (stats.accepted > 0) totalShares.accepted = stats.accepted;
            if (stats.rejected > 0) totalShares.rejected = stats.rejected;
            $('sharesAcc').textContent = totalShares.accepted;
            $('sharesRej').textContent = totalShares.rejected;
            
            if(startTime){
                var elapsedSecs = Math.floor((Date.now() - startTime) / 1000);
                var h = Math.floor(elapsedSecs / 3600);
                var m = Math.floor((elapsedSecs % 3600) / 60);
                var s = elapsedSecs % 60;
                $('uptimeVal').textContent = (h > 0 ? h + 'h ' : '') + m + 'm ' + s + 's';
            }
        } catch(e){
            log('Stats parse error: ' + e.message, 'warn');
        }
    }
}

function startMining(){
    if(mining) return;
    
    var wallet = $('wallet').value.trim();
    if(!wallet || wallet.length < 90){
        log('Please enter a valid Monero wallet address (95+ chars starting with 4 or 8)', 'error');
        return;
    }
    if(wallet[0] !== '4' && wallet[0] !== '8'){
        log('Wallet address must start with 4 (standard) or 8 (subaddress)', 'error');
        return;
    }
    
    var host = $('poolHost').value.trim() || 'pool.supportxmr.com';
    var port = parseInt($('poolPort').value) || 3333;
    var threads = Math.max(1, Math.min(8, parseInt($('threads').value) || 2));
    var worker = $('worker').value.trim() || 'android_miner';
    
    // Save config
    saveConfig('miner_host', host);
    saveConfig('miner_port', String(port));
    saveConfig('miner_wallet', wallet);
    saveConfig('miner_threads', String(threads));
    saveConfig('miner_worker', worker);
    
    mining = true;
    startTime = Date.now();
    totalShares = {accepted: 0, rejected: 0};
    
    $('statusText').textContent = 'MINING';
    $('statusText').className = 'status-value status-mining';
    $('btnStart').disabled = true;
    $('btnStop').disabled = false;
    $('hashrateVal').textContent = '0 H/s';
    $('sharesAcc').textContent = '0';
    $('sharesRej').textContent = '0';
    
    log('Connecting to ' + host + ':' + port + '...');
    log('Wallet: ' + wallet.substring(0,10) + '...' + wallet.substring(wallet.length-6));
    log('Threads: ' + threads + ' | Worker: ' + worker);
    
    if(window.MinerBridge && window.MinerBridge.startMining){
        var result = window.MinerBridge.startMining(host, port, wallet, worker, threads);
        if(result){
            log('✅ Native miner started successfully');
        } else {
            log('❌ Failed to start native miner – check pool and wallet', 'error');
            mining = false;
            $('statusText').textContent = 'ERROR';
            $('statusText').className = 'status-value status-error';
            $('btnStart').disabled = false;
            $('btnStop').disabled = true;
            return;
        }
    } else {
        log('⚠️ Native bridge not available', 'warn');
    }
    
    // Stats polling every 3 seconds
    statsTimer = setInterval(updateStats, 3000);
}

function stopMining(){
    if(!mining) return;
    mining = false;
    
    if(window.MinerBridge && window.MinerBridge.stopMining){
        window.MinerBridge.stopMining();
    }
    
    if(statsTimer){ clearInterval(statsTimer); statsTimer = null; }
    
    $('statusText').textContent = 'STOPPED';
    $('statusText').className = 'status-value status-ready';
    $('btnStart').disabled = false;
    $('btnStop').disabled = true;
    $('hashrateVal').textContent = '0 H/s';
    
    log('⏹ Miner stopped');
}

// Load saved config on page load
(function(){
    var h = loadConfig('miner_host');
    if(h) $('poolHost').value = h;
    var p = loadConfig('miner_port');
    if(p) $('poolPort').value = p;
    var w = loadConfig('miner_wallet');
    if(w) $('wallet').value = w;
    var t = loadConfig('miner_threads');
    if(t) $('threads').value = t;
    var wk = loadConfig('miner_worker');
    if(wk) $('worker').value = wk;
    
    log('✅ Monero RandomX Miner initialized');
    log('Enter your wallet address and press START MINING.');
    log('Recommended pool: pool.supportxmr.com:3333');
    
    if(window.MinerBridge && window.MinerBridge.isNativeLoaded && window.MinerBridge.isNativeLoaded()){
        log('✅ Native RandomX engine loaded');
    } else {
        log('⚠️ Waiting for native engine...', 'warn');
    }
})();
