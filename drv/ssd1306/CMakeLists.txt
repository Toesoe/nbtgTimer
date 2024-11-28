add_library(SSD1306
    src/ssd1306.c
    src/ssd1306_fonts.c
)

target_include_directories(SSD1306 PUBLIC
    inc
)