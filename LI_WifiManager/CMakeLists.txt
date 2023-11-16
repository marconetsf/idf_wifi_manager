
idf_component_register(SRC_DIRS src
    REQUIRES esp_wifi esp_netif log nvs_flash mdns wpa_supplicant lwip esp_http_server json
    INCLUDE_DIRS src
    EMBED_FILES src/pages/login.html
    src/pages/style.css
    src/pages/home.html
    src/pages/settings.html
    src/pages/overview.html
    src/pages/scripts.js)