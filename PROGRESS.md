# Progreso de Wind Waker nativo en PC

Objetivo: arrancar por el menú original, elegir o crear una partida, introducir
el nombre, reproducir la introducción y completar Isla Initia hasta embarcar.
La versión de PS Vita queda aplazada hasta conseguirlo en PC.

**La isla completa todavía no está validada.** Ya se ha comprobado el arranque
normal hasta recuperar el control y caminar con Link, sin saltar la historia.
Las tareas posteriores de la isla y el embarque siguen pendientes.

## Entorno de verificación

- Linux / WSLg, ejecutable x86 de 32 bits, GCC 15, SDL2 y OpenGL con llvmpipe.
- Imagen propia de GZLE01 Rev 0; este repositorio no incluye datos del juego,
  partidas, archivos extraídos ni grabaciones con contenido del disco.
- Cada prueba usa una tarjeta de memoria separada en `run-pc/probe-*/card`.
- Las modificaciones anteriores del proyecto se conservan. Los cambios en
  las dependencias se distribuyen como parches reproducibles.

## Resultados comprobados

| Prueba | Resultado | Límite de la comprobación |
|---|---|---|
| Compilación PC con datos reales | Superada con los parches actuales | No demuestra que la historia sea completable |
| Inicio directo de diagnóstico en Isla Initia | Link camina, gira, se detiene, rueda y sigue la altura del terreno | Usa `--outset`: salta menú e introducción y desactiva escenas automáticas |
| Agua en la isla | Se alcanzan estados de natación y reaparece en la isla | No es una prueba de progreso de la historia |
| Cámara | Seguimiento y recentrado con L funcionan | La cámara manual con C-stick está sin implementar |
| Estabilidad del movimiento | Pruebas de 120 y 75 segundos tras corregir el temporizador y las animaciones | No valida todas las zonas ni interacciones |
| Arranque normal, nombre, historia y despertar | Superada la secuencia completa y entregado el control; Link recorre 166,342 unidades con entrada de teclado | Diagnóstico sin ventana y con audio mudo; no valida las tareas posteriores |
| Salida de audio real | SDL abre a 32 kHz; la grabación contiene señal sin saturación en el tramo medido | Se han observado interrupciones; faltan funciones de música y efectos |
| Pruebas numéricas de temporizador y animación | Superadas | Casos de cálculo, no prueba de juego completo |
| Regresión del mezclador DSP | Superada: retornos, filtros, realimentación, liberación y osciladores | No comprueba fidelidad de Wind Waker frente a la consola |
| Aplicación de parches en fuentes limpias | Superada: 44 de tww y 4 de sms-port | No reemplaza los árboles locales existentes |
| Lector JStudio con datos sintéticos big-endian | Superada | Comprueba cabecera, alineación, identificadores de animación y manos |

Los registros y capturas completos se guardan localmente en los directorios
ignorados `run-pc/probe-*`. Las referencias siguientes permiten identificar
los ensayos; no son archivos disponibles en un clon del repositorio.

- `probe-iKgj7L`: estabilidad y movimiento, 120 s.
- `probe-dDRGgN`: animación y movimiento, 75 s.
- `probe-w2HnTO`: introducción completa; fallo de animación al empezar `awake`.
- `probe-8QQnHJ` y `probe-qPQyMp`: se supera esa carga tras el parche 0037,
  pero falla la lectura de parámetros de las manos. El segundo usa ventana
  y salida de sonido real.
- `probe-qPQyMp`: muestra de 111,936 s de PCM, pico absoluto 8384 y cero
  muestras saturadas. Esto **no** descarta cortes ni demuestra fidelidad sonora.

## Cambios recientes y problemas encontrados

1. Cabeceras de audio y mensajes: corregidas lecturas de datos big-endian.
2. Creación de actores: se detectan 82 perfiles con creación todavía vacía en
   la decompilación. Omitirlos permite investigar otras partes, pero deja NPC
   y enemigos ausentes; habrá que implementar los necesarios para la historia.
3. Temporizador de presentación: sustituido un bucle que podía bloquearse
   por un cálculo acotado, comprobado frente al comportamiento original.
4. Interpolación de animaciones: añadida la alternativa C++ para una rutina
   que solo tenía ensamblador de GameCube; se corrige la postura de Link.
5. Escena inicial: se corrige un acceso directo a un identificador de animación
   que ignoraba el lector big-endian. Después aparece otro fallo: el tamaño
   del identificador dentro de una cabecera JStudio se leía como nativo y
   desplazaba el puntero fuera de los parámetros. El parche 0038 corrige esa
   cabecera. El parche 0039 corrige también las cabeceras, desplazamientos,
   metadatos y etiquetas de espera/tamaño de los mensajes de escenas. El parche
   0041 recupera PAUSE y la rama de cámara fija absoluta necesaria para terminar
   `awake`; otras cámaras, incluidas las relativas, siguen pendientes. La prueba
   normal completa ya pasa hasta entregar el control y mover a Link.
6. Audio: la cola SDL se ha vaciado durante pruebas, de forma compatible con
   los cortes oídos. Se llena la cola inicial antes de reproducir y se
   añaden estadísticas de interrupciones con `SMS_AUDIO_STATS=1`. La prueba
   `probe-oDQ0o0`, sin depurador pero concurrente con otro diagnóstico, registra
   tres interrupciones iniciales: 331 ms en total, la mayor de 279 ms; el
   contador permanece estable en los siguientes intervalos observados. El
   llenado inicial no resuelve por sí solo los cortes durante las cargas.
   El parche 0040 recupera la ruta común de efectos de menú/diálogo no
   posicionales (0x0800–0x083f), con los parámetros contrastados con el código
   original. Se crean manejadores de sonido válidos en la prueba de arranque.
   `JAIZelBasic::bgmStart` y las demás ramas de `seStart` siguen pendientes;
   el DSP HLE tampoco
   implementa el comando de retardo Dolby `0x8d`. El audio no se considera resuelto.

## Hito: arranque normal hasta controlar a Link

`probe-U7Jmuo` termina con resultado 0 y un mensaje explícito de éxito:

```text
[flow] Link stage=sea pos=(-195402.0, 1650.0, 313668.0) proc=4 demo=0/0
[flow] control handed to Link; testing movement
[flow] normal-start movement distance=166.342
[flow] SUCCESS: normal startup, named new game, full opening, control and movement
```

El conductor introduce el nombre y avanza los diálogos con botones; deja que
se reproduzca toda la historia ilustrada. No modifica posiciones, estados de
escena ni banderas de progreso. La prueba reutiliza una tarjeta de diagnóstico
sin partidas previas y crea una partida nueva en ella. No usa `--outset`.

Antes de las correcciones, `probe-Y93Dgb` ya había recorrido los diálogos de
`awake`, pero se quedaba esperando la cámara final FIXEDFRM. Ese bloqueo se
resuelve con la rama de cámara fija absoluta recuperada del código original.
Las ramas relativas y otros tipos de cámara todavía no están recuperados.

La regresión sintética de BMG falla con las cabeceras anteriores y pasa con
el parche 0039. La serie completa de 41 parches de tww y 3 de sms-port se ha
aplicado en copias limpias temporales de las fuentes fijadas.

Diagnóstico de sonido `probe-uUbjVb`: en el primer corte, el hilo del juego
está detenido en `gx::glt::mainWait` esperando `shaderSetup`; el hilo gráfico
está leyendo `programCacheLoad`. Esa espera no entrega las interrupciones
que necesita el audio. Se está preparando una corrección y deberá medirse
sin depurador antes de afirmar que los cortes están resueltos.

La [revisión actual del proyecto original](https://github.com/zeldaret/tww/tree/d217e2b5e1c1514e6f3f640835f83f01dd67aa72)
también conserva vacías las funciones principales de cámaras de eventos y
los puntos de entrada de audio citados; no se ha sustituido la dependencia
local por esa revisión.

## Seguimiento: audio y escena de la torre

El recorrido `probe-P48Du2`, con ventana y salida de audio, termina con éxito
hasta el control y movimiento inicial de Link. Registra tres interrupciones
de audio, 228 ms en total y 95 ms la mayor, durante una ejecución con GDB.
No demuestra reproducción continua ni corrección visual de la escena.

El parche de plataforma 0004 entrega interrupciones del sistema invitado
durante esperas de comandos gráficos ya publicados, sin mantener el mutex
gráfico. Los hilos auxiliares nativos no pueden entregar esas interrupciones;
la reserva del anillo de comandos sigue sin ser reentrante. Tras la corrección,
`probe-ozyhmy` completa 90 segundos sin GDB ni capturas, con audio real, sin
registrar interrupciones. Solo valida ese tramo, no toda la isla.

El parche 0042 recupera del código original las variantes de sonido de avance
y cierre de mensajes de escenas y sus reglas de supresión. Las trazas distinguen
el avance cinematográfico (0x08fd, banco 2/programa 52), la confirmación de menú
(0x0801, banco 2/programa 1) y la rupia (0x0826, banco 2/programa 24). La prueba
`check/test_ui_audio.cpp` pasa. **El usuario sigue percibiendo incorrecto el
sonido de avance**: identificar la secuencia correcta no valida muestras,
envolventes, afinación ni mezcla.

El parche 0043 corrige otra causa comprobada de distorsión: `DSPRES_FILTER`
y `DSPADPCM_FILTER` son tablas nativas de `u16` en Wind Waker, mientras que
el mezclador espera parejas empaquetadas en `u32`, primer coeficiente en la
mitad alta. Leerlas directamente invertía las parejas en x86, tanto los
predictores AFC como los coeficientes de remuestreo. Se adaptan en la entrega
al mezclador sin modificar las constantes del juego ni el ABI de Sunshine.
`check/test_dsp_tables.py` compila las constantes de producción y el mezclador
real: una trama AFC sintética produce exactamente las muestras calculadas;
el renderizado con cualquiera de las dos tablas invertidas difiere de la
referencia. `probe-QuRgAl` confirma en el ejecutable los valores recibidos
por el mezclador (`res[0]=0x0c3966ad`, `adpcm[1]=0x08000000`) y ejercita los
tres efectos de referencia sin salida audible. Falta la comparación auditiva
posterior: no se declara fidelidad completa por esta prueba numérica.

En esa inspección, los tres efectos usan filtro IIR identidad y paso bajo
desactivado. Los filtros de voz no implementados no explican por sí solos
la diferencia de estos efectos concretos.

También queda abierto el parpadeo comunicado en la escena 3D de la torre.
`probe-b5SQLh` vuelve a superar el arranque normal y movimiento, esta vez con
300 capturas consecutivas distribuidas en cinco tramos de 60 fotogramas.
Usa reloj determinista y no valida audio. Es un diagnóstico parcial: no se
considera solucionado el parpadeo ni se ha capturado toda la escena.

`probe-gDDiXm` repite los cinco tramos con el reloj normal y también supera
el arranque y el movimiento. Las 300 muestras de cámara tienen valores
finitos, campo de visión válido y planos cercano/lejos ordenados. Los dos
cambios grandes entre imágenes consecutivas de esos tramos corresponden a
cambios de plano; no se ha reproducido una alternancia rápida en esas muestras.
Esto no descarta fallos fuera de los tramos ni en la presentación de la ventana.

### Alcance de la primera isla

Abril, la abuela, Orca, Tetra, los Bokoblin, el Helmaroc de esta parte y el
barco pirata tienen cuerpos de código implementados en la revisión fijada.
Esto permite mantener el objetivo de llegar al embarque, pero no prueba que
sus interacciones funcionen en PC. Faltan funciones de cámaras de conversación,
obtención de objetos y otras escenas; también faltan NPC secundarios y el
actor compartido por Zuko, Niko y Mako. Todavía no se ha demostrado un límite
obligatorio anterior al embarque por una ausencia concreta de decompilación.

El límite verificado sigue siendo el control inicial de Link. Conversaciones,
ropa, catalejo, entrenamiento, espada, escudo, bosque, rescate, transiciones y
despedida deben probarse en orden, sin dar por superados los eventos que se
omitan o desbloqueen artificialmente.

**Primer bloqueo reproducido después de obtener el control:** `probe-ZKKyqJ`
supera de nuevo el arranque y el movimiento con el parche de audio 0043.
Se acerca a Abril usando teclado, activa `DEFAULT_TALK` (evento 106) y entra
en la cámara `TALK` (corte 1146). El mensaje 3033 (`0x0bd9`) permanece en
`MSG_PREPARING`, y Link queda en modo de conversación. Se detuvo el diagnóstico
con SIGINT tras observar el estado repetido; resultado 255, **no superado**.
`dMsg_initProc` espera la señal de cámara preparada; `talktoEvCamera` y
`talktoCamera` siguen vacías y no producen esa señal. Recuperar esas cámaras
es necesario antes de poder validar esta primera conversación. No se ha
forzado la señal ni marcado el diálogo como completado para continuar.

`probe-exvnqW` recoge 3206 fotogramas consecutivos desde la transición a la
isla hasta abrir esa conversación, incluyendo toda la escena `awake` y el
movimiento inicial. Las dos vistas inicialmente no preparadas producen
imágenes negras durante la transición; las restantes tienen perspectiva válida.
La revisión de cambios grandes de imagen muestra cortes de escena y giros de
cámara al controlar a Link, sin reproducir una alternancia continua de destellos
en el XFB. Esta ejecución se detuvo por una lectura de diagnóstico incorrecta
al entrar en el segundo tipo de mensajes, no por un fallo nativo del juego.
El observador se corrigió distinguiendo mensajes cinemáticos y de juego;
`probe-ZKKyqJ` pudo entonces observar el bloqueo de conversación real.

## Verificación visible y cierre al entrar en casa

El usuario confirma tras el parche 0043 que los efectos del menú de inicio
y de avance de mensajes ya suenan correctos, y que ha desaparecido el
parpadeo de la cinemática. Es una comprobación perceptiva del usuario en
la ejecución visible, adicional a las regresiones numéricas anteriores.
No implica que estén implementados todos los efectos, voces o música.

Quedan observados en esa misma prueba: voces de Link ausentes al saltar y
rodar, desfase del efecto de la voltereta, cámara manual no disponible y
pequeños tirones en las olas al romper en la orilla. `linkVoiceStart` y
`getLinkVoiceVowel` están vacías; recuperar su implementación es una tarea
concreta. El desfase y los tirones aún no tienen causa verificada.

Al entrar por la puerta de la abuela, el registro solicita `LinkRM`, punto 1,
habitación 0, wipe 10 y termina con SIGFPE en `JUTFader::control`, línea 40:
división entera por cero. `dOvlpFd_FadeIn` solicita legítimamente
`startFadeOut(0)` al completar la transición. El parche 0044 permite terminar
ese fundido en la actualización actual sin dividir por cero; conserva los
cálculos de los fundidos de duración positiva.

`check/test_fader.py` compila el `JUTFader.cpp` de producción, sustituyendo
solo el contexto gráfico por un doble sin dibujo. Antes del parche falla con
SIGFPE; después pasa las duraciones 0 a 30 en ambos sentidos, sus valores de
alfa, la aceptación/rechazo por estado y un fundido instantáneo diferido.
La compilación PC pasa y los 44 parches se aplican sobre fuentes limpias.

`probe-dvgrnb` termina con resultado 0 y éxito explícito: completa una
solicitud de transición a `LinkRM` con ese punto y wipe, carga el interior y
Link recorre 51,916 unidades mediante teclado. **Es una prueba aislada:**
parte de `--outset` y el depurador escribe la solicitud de próximo escenario;
no valida el uso de la puerta, la conversación con la abuela ni el recorrido
normal de la historia. `tools/room_transition.gdb` permite repetirla.
Los intentos previos `probe-KseqH0` y `probe-73RZUS` terminan por tiempo
límite/terminación forzada del depurador (137): llamar a funciones del invitado
desde ese punto de diagnóstico no permitió completar la prueba; no se cuentan
como fallos nativos de carga ni como ensayos superados.

Se abre otra ejecución visible desde la playa con audio real y tarjeta
separada (`run-pc/visible-house-card`) para contrastar la entrada por la puerta.
La partida del usuario en `run-pc/card-a` se conserva. Esta ruta de diagnóstico
desactiva escenas automáticas; no sustituye la prueba de progreso de la historia.

## Cómo repetir las comprobaciones

```sh
./build-pc.sh
python3 check/test_animation.py
python3 check/test_jstudio.py
python3 check/test_dsp_tables.py
python3 check/test_fader.py
g++ -std=c++11 -O2 -Iport/include check/test_timing.cpp -o /tmp/tww-timing-test
/tmp/tww-timing-test

# Recorrido original mediante pulsaciones, sin inyectar escenas ni posiciones:
./tools/probe-pc.sh --audio --seconds 420 --gdb "$PWD/tools/normal_flow.gdb"

# Diagnóstico sin ventana, con reloj determinista; no valida audio audible:
./tools/probe-pc.sh --headless --deterministic --seconds 300 \
  --gdb "$PWD/tools/normal_flow.gdb"

# Cinco tramos de 60 fotogramas consecutivos y cámara activa en camera.jsonl:
TWW_CAPTURE_TOWER=sample ./tools/probe-pc.sh --headless --seconds 420 \
  --gdb "$PWD/tools/normal_flow.gdb"

# Transición aislada a la casa; no prueba la puerta ni la historia:
./tools/probe-pc.sh --outset --headless --seconds 120 \
  --gdb "$PWD/tools/room_transition.gdb"
```

`TWW_CAPTURE_TOWER=all` captura hasta los primeros 4000 fotogramas de la isla
con cámara creada (aproximadamente 3,5 GiB a la resolución actual). Las capturas
y el depurador alteran el rendimiento: no usar este modo para medir cortes de
audio. Este diagnóstico solo cambia la frecuencia de captura, no la cámara
ni el progreso de la partida.

El conductor deja reproducirse la historia ilustrada completa. Un tiempo
límite (estado 124) no es un resultado satisfactorio por sí solo: hay que leer
el registro. La opción `--audio` añade un WAV y estadísticas de interrupciones;
la grabación de las muestras mezcladas no incluye necesariamente las pausas
que produce quedarse sin datos en el dispositivo de salida.

## Próximos hitos

- Vigilar regresiones del parpadeo ya descartado por el usuario en la torre;
  diagnosticar los tirones de las olas y el desfase del efecto de rodar.
- Corregir cámaras, diálogos y actores que bloqueen las tareas de la isla.
- Comprobar espada, escudo, bosque y rescate, cambios de habitación, regreso,
  despedida y embarque. Ninguno se considera validado todavía.
- Medir y corregir cortes de sonido sin usar el depurador para la prueba final;
  recuperar música, efectos y voces necesarios y comprobar sus transiciones.
- Publicar cada avance probado con comandos, resultado y fallos pendientes.

No hay todavía una estimación fiable de sesiones: faltan partes del código
original, además de errores de adaptación a PC. La estimación se revisará
cuando se complete el primer recorrido real y se identifiquen sus bloqueos.
