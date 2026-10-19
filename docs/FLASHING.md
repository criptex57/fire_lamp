# Сборка и прошивка

Для готовой сборки: **NodeMCU v3 / ESP8266, flash 4 МБ, WS2812B 16×16**,
DATA на D4/GPIO2, сенсор на D2/GPIO4. Перед подключением USB прочитайте
[раздел о питании](HARDWARE.md#nodemcu-питание-и-usb).

## Вариант 1: PlatformIO IDE

1. Установите [VS Code](https://code.visualstudio.com/) и расширение **PlatformIO IDE**.
2. Клонируйте проект и откройте его папку в VS Code:

   ```sh
   git clone https://github.com/criptex57/fire_lamp.git
   cd fire_lamp
   ```

3. Подключите NodeMCU USB-кабелем с передачей данных.
4. В PlatformIO выберите окружение **nodemcuv2** и запустите **Build**, затем **Upload**.
5. Откройте Serial Monitor на **9600 бод**. После запуска появится ID и адрес лампы.

Интернет нужен при первоначальной установке платформы, toolchain и библиотек.
Для работы уже прошитой лампы интернет не требуется.

## Вариант 2: PlatformIO в терминале

Можно установить PlatformIO в отдельное Python-окружение:

```sh
python3 -m venv .venv
```

Активируйте его:

```sh
# macOS / Linux
source .venv/bin/activate
```

```powershell
# Windows PowerShell
.venv\Scripts\Activate.ps1
```

Затем:

```sh
python -m pip install platformio
pio run -e nodemcuv2
pio device list
pio run -e nodemcuv2 -t upload
pio device monitor -b 9600
```

При нескольких USB-устройствах укажите порт явно:

```sh
# macOS: подставьте порт из pio device list
pio run -e nodemcuv2 -t upload --upload-port /dev/cu.usbserial-XXXX
pio device monitor --port /dev/cu.usbserial-XXXX -b 9600
```

```sh
# Linux
pio run -e nodemcuv2 -t upload --upload-port /dev/ttyUSB0
```

```powershell
# Windows
pio run -e nodemcuv2 -t upload --upload-port COM5
```

На Windows, если команда `python3` не найдена, используйте `py -m venv .venv`.
9600 — скорость диагностических сообщений нашей прошивки, а не требование к
скорости записи. Сообщения загрузчика ESP8266 при 9600 могут выглядеть нечитаемо.
Закройте Serial Monitor перед повторной загрузкой: порт должен быть свободен.

Профиль для Wemos D1 mini:

```sh
pio run -e d1_mini
pio run -e d1_mini -t upload
```

Основная проверенная конфигурация — NodeMCU; перед использованием другой платы
сверяйте распиновку и flash.

## Вариант 3: готовый бинарник

В репозитории есть [fire-lamp-nodemcu-16x16.bin](../firmware/fire-lamp-nodemcu-16x16.bin)
для базовой конфигурации. Его загружают **по адресу flash 0x0**.
После установки пакета `esptool` можно использовать:

```sh
python -m pip install esptool==4.8.1
python -m esptool --chip esp8266 --port PORT --baud 115200 write_flash 0x0 firmware/fire-lamp-nodemcu-16x16.bin
```

Замените `PORT` на реальный порт. Не требуется `erase_flash` для обычного обновления:
настройки приложения хранятся отдельно. Для сброса используйте сенсор 10 секунд.
Новая прошивка не включает пароль вашей домашней сети: его вводят при настройке.

Готовый файл не меняется при правке `config.h`: после такой правки нужно заново
собрать исходники. Результат сборки находится в `.pio/build/nodemcuv2/firmware.bin`.

## Что встраивается автоматически

В [platformio.ini](../platformio.ini) закреплены платформа и версии библиотек.
Скрипт [scripts/embed_web.py](../scripts/embed_web.py) превращает `web/index.html`
в `include/web_ui.generated.h` перед сборкой. Этот заголовок не коммитится,
потому что генерируется из веб-страницы. **Upload Filesystem запускать не нужно**:
LittleFS используется только для настроек самой лампы.

## Проверка после загрузки

1. Дождитесь успешной записи и проверки хеша загрузчиком.
2. Нажмите RESET, если плата не перезапустилась автоматически.
3. В Serial Monitor найдите строку вида `firelamp-xxxxxx http://...`.
4. Если домашний Wi-Fi не настроен, появится открытая сеть `firelamp-xxxxxx`,
   адрес **http://192.168.4.1**.
5. Перейдите к [настройке и управлению](USAGE.md).

## Если не прошивается

| Симптом | Что проверить |
| --- | --- |
| Порта нет | Кабель с данными, другой USB-порт, драйвер CH340 от производителя WCH |
| `Permission denied` на Linux | Права доступа пользователя к последовательному порту |
| `Port busy` | Закрыть Arduino IDE / Serial Monitor / другие программы с этим портом |
| Долго стоит `Connecting...` | Плата выбрана верно; при необходимости удержать FLASH, кратко нажать RESET и отпустить FLASH после начала подключения |
| Ошибки записи | Питание и кабель; отсоединить периферию и проверить плату отдельно |
| После прошивки неправильные цвета | Тип WS2812B и порядок GRB в `FastLED.addLeds` в `src/main.cpp` |
| Матрица повёрнута | Исправить конфиг ориентации и пересобрать |

Прошивка по Wi-Fi (OTA) в этой версии не реализована. Каждую лампу обновляют по USB.
