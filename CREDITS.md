# Credits

- **Walking sprites:** [PMDCollab SpriteCollab](https://github.com/PMDCollab/SpriteCollab)
  (Pokémon Mystery Dungeon sprite archive), licensed
  [CC BY-NC 4.0](https://creativecommons.org/licenses/by-nc/4.0/). Per-sprite artist
  credits are in each Pokémon's `credits.txt` in that repository; many sprites are
  original Spike Chunsoft assets. Downloaded at runtime, not redistributed here.
- **Pokémon data and fallback sprites:** [PokéAPI](https://pokeapi.co/) and
  [PokeAPI/sprites](https://github.com/PokeAPI/sprites). The bundled Pokédex
  (`src/pokemon/dex_data.inc`) is generated from PokéAPI's CSV data by `tools/gen_dex.py`.
- **Japanese/Chinese glyphs:** [GNU Unifont](https://unifoundry.com/unifont/) 16.0.04 (kana,
  CJK symbols, half/full-width forms, CJK Unified Ideographs), GPLv2+ with the GNU font
  embedding exception / SIL OFL 1.1; converted to `data/cjk16.bin` by `tools/gen_cjk_font.py`.
- **Lyrics:** [LRCLIB](https://lrclib.net/).
- **Artist genres:** Apple's [iTunes Search API](https://performance-partners.apple.com/search-api);
  the genre id → badge table (`src/ui/genre_data.inc`) is generated from Apple's genre tree by
  `tools/gen_genres.py`.
- **Libraries:** [TFT_eSPI](https://github.com/Bodmer/TFT_eSPI) (FreeBSD/MIT mix),
  [ArduinoJson](https://arduinojson.org/) (MIT), [TJpg_Decoder](https://github.com/Bodmer/TJpg_Decoder)
  (FreeBSD-style, tjpgd), [PNGdec](https://github.com/bitbank2/PNGdec) (Apache-2.0),
  [spotify-api-arduino](https://github.com/witnessmenow/spotify-api-arduino) (MIT),
  [XPT2046_Touchscreen](https://github.com/PaulStoffregen/XPT2046_Touchscreen) (MIT, hardware
  check only), [Unity](https://github.com/ThrowTheSwitch/Unity) (MIT, host tests).
- **Inspiration:** [intellij-pokemon-progress](https://github.com/kagof/intellij-pokemon-progress).
- Pokémon © Nintendo / Creatures Inc. / GAME FREAK inc. This is a personal,
  non-commercial project.
