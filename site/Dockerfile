# The alerse.app site: nginx serving the files build.py writes.
#   python site/build.py && docker build -t alerse-site site
FROM nginxinc/nginx-unprivileged:1.29-alpine
COPY nginx.conf /etc/nginx/conf.d/default.conf
COPY public/ /usr/share/nginx/html/
EXPOSE 8080
