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

  // The Russian site (ru.alerse.app) shares this script.
  var ru = root.lang === 'ru';
  function t(en, rus) { return ru ? rus : en; }

  document.addEventListener('DOMContentLoaded', function () {
    var toggle = document.querySelector('.toggle');
    if (toggle) {
      var label = function () {
        toggle.setAttribute('aria-label', current() === 'dark' ? t('Use the light theme', 'Светлая тема') : t('Use the dark theme', 'Тёмная тема'));
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
      windows: [t('Download for Windows', 'Скачать для Windows'), 'https://github.com/VizzleTF/alerse-win/releases/latest'],
      mac: [t('Download for macOS', 'Скачать для macOS'), 'https://github.com/VizzleTF/alerse-mac/releases'],
      linux: [t('Download for Linux', 'Скачать для Linux'), 'https://github.com/VizzleTF/alerse-linux/releases/latest']
    };
    document.querySelectorAll('[data-download]').forEach(function (a) {
      a.textContent = targets[platform][0];
      a.href = targets[platform][1];
    });

    document.querySelectorAll('.copy').forEach(function (button) {
      button.addEventListener('click', function () {
        var text = button.parentElement.querySelector('code').textContent;
        navigator.clipboard.writeText(text).then(function () {
          button.textContent = t('Copied', 'Скопировано');
          setTimeout(function () { button.textContent = t('Copy', 'Копировать'); }, 1500);
        });
      });
    });
  });
})();
