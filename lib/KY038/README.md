# KY038 – مكتبة Arduino لحساس الصوت KY-038

مكتبة احترافية للتعامل مع حساس الصوت الصغير **KY-038** (ميكروفون + مقارن LM393).

## التوصيل

| KY-038 | Arduino Uno | ESP32 |
|--------|-------------|-------|
| `+`    | 5V          | 3.3V  |
| `G`    | GND         | GND   |
| `A0`   | A0          | GPIO34 (ADC1) |
| `D0`   | D2 (اختياري) | GPIO27 (اختياري) |

> في ESP32 استخدم أطراف ADC1 (32–39) فقط؛ ADC2 لا يعمل مع الـ WiFi.

## التثبيت
Sketch ▸ Include Library ▸ Add .ZIP Library… ثم اختر `KY038.zip`.

## الميزات
- قياس **Peak-to-Peak / RMS / DC bias** (الإشارة الصوتية متذبذبة، لذلك قراءة واحدة لا تكفي).
- **معايرة** ضجيج الغرفة تلقائيًا (`calibrate()`).
- مستوى بنسبة % مع تنعيم (EMA)، وتقدير **dB** نسبي.
- **Hysteresis + Debounce** لأحداث `SOUND_START / SOUND_END`.
- كشف **التصفيق المفرد والمزدوج**.
- دعم الخرج الرقمي D0 مع **مقاطعة (Interrupt)**.
- وضع **Fast ADC** على AVR (أسرع ~3 مرات).
- يعمل على AVR / ESP32 / ESP8266 / SAMD / RP2040 / STM32.

## الاستخدام السريع
```cpp
#include <KY038.h>
KY038 mic(A0, 2);          // analog pin, digital pin (اختياري)

void setup() {
  Serial.begin(115200);
  mic.begin(10, 5.0);      // ADC bits, Vref
  mic.calibrate(2000);     // اترك الغرفة هادئة
}
void loop() {
  mic.update();            // لا تستخدم delay() في loop
  Serial.println(mic.level());
}
```

## أهم الدوال
| الدالة | الوصف |
|--------|-------|
| `begin(bits, vref)` | تهيئة الأطراف ودقة الـ ADC |
| `calibrate(ms)` | قياس ضجيج الخلفية والـ bias |
| `update()` | معالجة غير حاجبة، تُستدعى باستمرار |
| `measure(ms)` | قياس حاجب يعيد `Measurement` |
| `level()` / `rawLevel()` | المستوى % (منعّم / خام) |
| `isSoundActive()` | هل هناك صوت الآن |
| `dbFS()` / `dbAboveNoise()` | تقدير dB نسبي |
| `setThresholds(on, off)` | عتبتا التشغيل/الإيقاف بالنسبة المئوية |
| `setClapTiming(max, gapMin, gapMax)` | ضبط توقيت التصفيق |
| `setFullScale(p2p)` | قيمة p2p التي تعادل 100% |
| `onEvent(cb)` | تسجيل دالة استدعاء للأحداث |
| `digitalDetected()` | قراءة D0 مع مراعاة مستوى التفعيل |
| `attachDigitalInterrupt(isr)` | مقاطعة على D0 |
| `waitForSound(ms)` | انتظار صوت مع مهلة |

## ملاحظات مهمة
- الحساس **غير معاير**، فقيم dB نسبية وليست SPL حقيقية.
- لو أعطى D0 نتيجة معكوسة، استخدم `setDigitalActiveLevel(LOW)`.
- لو كانت الحساسية ضعيفة أو زائدة عدّل `setFullScale()` و`setThresholds()` ومقاومة الحساسية على الموديول.
- لكشف التصفيق بدقة، لا تحجب `loop()` بـ `delay()`.

الرخصة: MIT
