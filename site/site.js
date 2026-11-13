// Theme toggle, platform detection for the download button, copy buttons.
// No requests, no cookies; the theme choice stays in this browser only.
(function () {
  var root = document.documentElement;
  function stored() { try { return localStorage.getItem('theme'); } catch (e) { return null; } }
  function store(v) { try { localStorage.setItem('theme', v); } catch (e) {} }
  var saved = stored();
  if (saved === 'light' || saved === 'dark') root.setAttribute('data-theme', saved);

  function current() {
    var t = root.getAttribute('data-theme');
    if (t) return t;
    return matchMedia('(prefers-color-scheme: dark)').matches ? 'dark' : 'light';
  }

  document.addEventListener('DOMContentLoaded', function () {
    var toggle = document.querySelector('.toggle');
    if (toggle) {
      var label = function () {
        toggle.setAttribute('aria-label', current() === 'dark' ? 'Use the light theme' : 'Use the dark theme');
      };
      label();
      toggle.addEventListener('click', function () {
        var next = current() === 'dark' ? 'light' : 'dark';
        root.setAttribute('data-theme', next);
        store(next);
        label();
      });
    }

    // The hero's button names the visitor's platform; every other platform
    // stays one link away.
    var ua = navigator.userAgent;
    var platform = /Mac/.test(ua) && !/iPhone|iPad/.test(ua) ? 'mac' : /Linux/.test(ua) && !/Android/.test(ua) ? 'linux' : 'windows';
    var targets = {
      windows: ['Download for Windows', 'https://github.com/VizzleTF/alerse-win/releases/latest'],
      mac: ['Download for macOS', 'https://github.com/VizzleTF/alerse-mac/releases'],
      linux: ['Download for Linux', 'https://github.com/VizzleTF/alerse-linux/releases/latest']
    };
    document.querySelectorAll('[data-download]').forEach(function (a) {
      a.textContent = targets[platform][0];
      a.href = targets[platform][1];
    });

    document.querySelectorAll('.copy').forEach(function (button) {
      button.addEventListener('click', function () {
        var text = button.parentElement.querySelector('code').textContent;
        navigator.clipboard.writeText(text).then(function () {
          button.textContent = 'Copied';
          setTimeout(function () { button.textContent = 'Copy'; }, 1500);
        });
      });
    });
  });
})();
