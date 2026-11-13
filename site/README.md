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
- The privacy page says the server keeps request logs at most 7 days: whoever runs it
  must keep to that.
- Version numbers on the pages are written in `build.py`; change them with each release.
