# The alerse.app site: nginx serving the files build.py writes.
#   python site/build.py && docker build -t alerse-site site
#   python site/build.py --lang ru && docker build -t alerse-site-ru #     --build-arg PAGES=public-ru --build-arg CONF=nginx-ru.conf site
FROM nginxinc/nginx-unprivileged:1.29-alpine
ARG PAGES=public
ARG CONF=nginx.conf
COPY ${CONF} /etc/nginx/conf.d/default.conf
COPY ${PAGES}/ /usr/share/nginx/html/
EXPOSE 8080
