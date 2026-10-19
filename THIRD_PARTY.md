# Исходники эффекта

Огонь адаптирован из [AlexGyver/GyverLamp2](https://github.com/AlexGyver/GyverLamp2),
commit `145fbfb99bd732e7a6dc37b8fc77da38245f936f`:

- `firmware/GyverLamp2/fire2020.ino`: «Огонь 2020», (c) SottNick;
  Perlin noise fire procedure by Yaroslaw Turbin.
  Ссылки из исходного файла: https://pastebin.com/RG0QGzfK и
  https://www.reddit.com/r/FastLED/comments/hgu16i/my_fire_effect_implementation_based_on_perlin/.
- `firmware/GyverLamp2/palettes.h`: градиент `Fire_gp`.

Сохранены вычисление шума, вертикальное затухание и движение искр.
Массивы привязаны к размерам матрицы; удалены зависимости от пресетов,
звука и глобальных структур исходной прошивки. Используется фиксированная
палитра Fire_gp, масштаб 70 и период кадров 35 мс. Это вариант «Огонь 2020»,
а не второй эффект `fire2D.ino`; синхронизация кадров между лампами не реализована.

В просмотренной версии upstream нет отдельного файла LICENSE. Эта ссылка и
атрибуция не являются разрешением на распространение: перед публикацией
производной прошивки нужно уточнить условия у правообладателей.

FastLED 3.6.0 и ArduinoJson 6.21.5 устанавливаются PlatformIO со своими лицензиями.

## Дополнительные эффекты

`include/effects.h`, Hiphotic / Plasma / Aurora:
адаптированы по [WLED wled00/FX.cpp](https://github.com/wled/WLED/blob/7321c3bfd646184eb66e2142aa5a58bf8ff88289/wled00/FX.cpp),
commit `7321c3bfd646184eb66e2142aa5a58bf8ff88289` (проверен 2026-10-19).
Copyright (c) 2016-present Christian Schwinne and individual WLED contributors.
Эти адаптированные части предоставляются по **EUPL-1.2-or-later**;
полный текст: [licenses/WLED-EUPL-1.2.txt](licenses/WLED-EUPL-1.2.txt).
Hiphotic: ldirko, modified by Andrew Tuline;
[оригинальная ссылка из WLED](https://editor.soulmatelights.com/gallery/810).
Plasma: Andrew Tuline / [FastLED-Demos](https://github.com/atuline/FastLED-Demos/blob/master/plasma/plasma.ino).
Aurora: адаптация алгоритма жизненного цикла волн WLED AuroraWave.

Изменения: удалён движок сегментов WLED; RGB вместо RGBW; фиксированные палитры
и скорость; координаты через конфиг разводки матрицы; Plasma разделена на X/Y,
Aurora проецируется на двумерные ленты. Полный код адаптаций находится в проекте.
Отзывы о WLED относятся к оригинальным эффектам, визуальный результат адаптаций
может отличаться в зависимости от матрицы, рассеивателя и формы корпуса.

Pacifica адаптирована из
[FastLED 3.6.0 examples/Pacifica/Pacifica.ino](https://github.com/FastLED/FastLED/blob/3.6.0/examples/Pacifica/Pacifica.ino).
Mark Kriegsman and Mary Corey March, December 2019, for Dan.
MIT; [licenses/FastLED-MIT.txt](licenses/FastLED-MIT.txt).
Сохранены три палитры, четыре независимых слоя, пенные блики и затемнение
синего/зелёного; развёртка слоёв изменена с ленты на двумерную матрицу.

## Дополнительные варианты огня

`include/fire_variants.h`: Fire2012, Mark Kriegsman, July 2012,
из [FastLED 3.6.0 Fire2012.ino](https://github.com/FastLED/FastLED/blob/3.6.0/examples/Fire2012/Fire2012.ino).
MIT, текст в `licenses/FastLED-MIT.txt`. Изменения: независимые столбцы матрицы,
зона поджига ограничена четырьмя нижними рядами.
