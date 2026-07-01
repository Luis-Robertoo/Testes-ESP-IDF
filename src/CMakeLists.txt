# Pega todos os arquivos da pasta automaticamente
FILE(GLOB_RECURSE app_sources ${CMAKE_SOURCE_DIR}/src/*.*)

# Registra os arquivos encontrados E as bibliotecas (componentes) necessárias
idf_component_register(SRCS ${app_sources}
                       INCLUDE_DIRS "."
                       REQUIRES esp_websocket_client nvs_flash)