# TsundereGram Desktop

Неофициальный клиент Telegram для Windows от команды **TsundereGram** — десктопная пара к Android-версии.
Основан на [AyuGram Desktop](https://github.com/AyuGram/AyuGramDesktop) (Telegram Desktop), GPLv3.

## Скачать

- **Установщик:** [TsundereGram-Desktop-Setup-x64.exe](https://github.com/warashl/TsundereGram-Desktop/releases/latest/download/TsundereGram-Desktop-Setup-x64.exe)
- **Портативная версия:** [TsundereGram-Desktop-x64.zip](https://github.com/warashl/TsundereGram-Desktop/releases/latest/download/TsundereGram-Desktop-x64.zip)
- Сайт: https://warashl.github.io/

## Что внутри

- Режим призрака: без «прочитано», без «в сети», без «печатает».
- Сохранение удалённых и истории изменённых сообщений.
- Значки TsundereGram (разработчик / малыш / свои), градиентные ники, баннеры профиля — общие с Android-версией (облачный конфиг TsundereGram).
- Фильтры сообщений, режим стримера, локальный Premium, переводчик, свои иконки приложения и многое другое из AyuGram.

## Как это собирается

Репозиторий содержит только наши изменения: `scripts/apply.py` накладывает брендинг и функции TsundereGram
поверх исходников AyuGram Desktop (зафиксированный коммит в `.github/workflows/windows.yml`), а GitHub Actions
собирает Windows x64 и публикует релиз.

Лицензия: GPLv3 (как у Telegram Desktop и AyuGram Desktop).
