# The alerse.app site

Five static pages (home, download, sync, privacy, terms) with one stylesheet and one
small script; nothing is loaded from anyone else and no cookie is set. The design is the
Pen file and its brief, outside this repository.

```sh
python site/build.py                      # writes site/public
docker build -t alerse-site site          # nginx serving site/public on port 8080
docker run --rm -p 8080:8080 alerse-site  # http://localhost:8080
```

- `build.py` holds the pages' text; edit it, not `public/`, which is rebuilt.
- `nginx.conf` maps `/download` to `download.html` and sets the security headers
  (a CSP allowing only the site itself, HSTS, no referrer).
- nginx keeps no request logs (`access_log off`), as the privacy page says; www.alerse.app
  redirects to alerse.app.
- Version numbers on the pages are written in `build.py`; change them with each release.
- The Russian site for ru.alerse.app: `python site/build.py --lang ru` writes `public-ru`
  from the same markup, with the phrases of `ru.py`; the build stops when a phrase is no
  longer in `build.py`. Its image takes `--build-arg PAGES=public-ru --build-arg
  CONF=nginx-ru.conf`. Each page links its counterpart on the other site.
