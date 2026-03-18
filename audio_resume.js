// Wait for miniaudio to fully initialise, then resume the AudioContext
// on the first user interaction. Uses a delayed poll so we don't fire
// the onaudioprocess callback before the output buffers are ready.
(function () {
    var resumed = false;

    function tryResume() {
        if (resumed) return;
        // miniaudio stores its context in this global once ready
        if (typeof window.miniaudio !== 'undefined' && window.miniaudio.device_count > 0) {
            for (var i = 0; i < window.miniaudio.devices.length; i++) {
                var dev = window.miniaudio.devices[i];
                if (dev && dev.webaudio && dev.webaudio.state === 'suspended') {
                    dev.webaudio.resume().then(function () { resumed = true; });
                }
            }
        }
    }

    // Poll until miniaudio is ready, then attach interaction listeners
    var pollId = setInterval(function () {
        if (typeof window.miniaudio !== 'undefined') {
            clearInterval(pollId);
            ['click', 'keydown', 'touchstart'].forEach(function (evt) {
                window.addEventListener(evt, function () {
                    setTimeout(tryResume, 300);
                }, { once: true });
            });
        }
    }, 100);
})();
