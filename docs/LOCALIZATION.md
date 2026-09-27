# Aozora Favorites localization

The English package is the only core package. It contains one DLL, one SWF,
one ESP, the English MCM config, and `Interface/Translations/AozoraFavorites_en.txt`.

The Chinese package is an overlay only. Install it after the English package in
MO2. The current profile uses `sLanguage=en`, so the patch deliberately
overrides `Interface/Translations/AozoraFavorites_en.txt` with Chinese values.
It does not ship a DLL, SWF, ESP, or scripts.

All Scaleform and MCM display strings use `$AOZORA_*` keys. The active Fallout 4
language selects the matching translation file. Internal item categories use
stable identifiers (`WEAP`, `ARMO`, `ALCH`, `OTHER`) and are not localized.
