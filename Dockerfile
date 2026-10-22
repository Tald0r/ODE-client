# The browser client as a container: the WebAssembly build served by nginx,
# which also proxies /game to the WebSocket gateway so the page and its
# socket share one origin. The proprietary asset pack is not in the image;
# mount it at /usr/share/nginx/html/assets (docs/browser/browser-client.md).
#
#   docker build -t darkeden-web .
#   docker run -p 127.0.0.1:18739:80 -e GATEWAY_UPSTREAM=host:8080 \
#       -v /path/to/assets:/usr/share/nginx/html/assets:ro darkeden-web
#
# docker/docker-compose.yml wraps this for the server repository's stack.

FROM emscripten/emsdk:6.0.10@sha256:e077d54e2b8970575ebc4f185ac1de0b95c05f2b266134d4ba27449af7aebf65 AS build
ARG WEB_BUILD_JOBS=8
COPY . /src
# The same script CI and tools/web/build.ps1 use; only the game is built.
RUN mkdir -p /work && WEB_BUILD_JOBS="$WEB_BUILD_JOBS" WEB_BUILD_TARGETS=DarkEden \
    bash /src/tools/web/build-container.sh

FROM nginx:1.27-alpine
COPY --from=build /work/build-wasm/bin/DarkEden.mjs /work/build-wasm/bin/DarkEden.wasm \
    /work/build-wasm/bin/index.html /work/build-wasm/bin/launcher.mjs \
    /work/build-wasm/bin/touch-controls.mjs /work/build-wasm/bin/asset-store.mjs /usr/share/nginx/html/
COPY docker/nginx.conf.template /etc/nginx/templates/default.conf.template
COPY docker/40-client-config.sh /docker-entrypoint.d/40-client-config.sh
RUN chmod +x /docker-entrypoint.d/40-client-config.sh && mkdir -p /usr/share/nginx/html/assets
# Only GATEWAY_* is substituted into the nginx template; nginx's own $variables stay.
ENV NGINX_ENVSUBST_FILTER="^GATEWAY_" \
    GATEWAY_UPSTREAM=odk-server:8080 \
    DARKEDEN_WEBSOCKET_URL=/game \
    DARKEDEN_LOGIN_HOST=127.0.0.1 \
    DARKEDEN_LOGIN_PORT=9999
EXPOSE 80
