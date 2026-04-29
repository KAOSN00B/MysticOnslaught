// The WebAudio context starts suspended. Resume it on the first user gesture.
// InitAudioDevice() runs at startup so window.miniaudio is ready immediately.
(function () {
    var resumed = false;

    function tryResume() {
        if (resumed) return;
        if (typeof window.miniaudio === 'undefined' || window.miniaudio.device_count === 0) return;
        for (var i = 0; i < window.miniaudio.devices.length; i++) {
            var dev = window.miniaudio.devices[i];
            if (dev && dev.webaudio) {
                dev.webaudio.resume();
                resumed = true;
            }
        }
    }

    ['mousedown', 'keydown', 'touchstart', 'pointerdown'].forEach(function (evt) {
        window.addEventListener(evt, tryResume);
    });
})();
