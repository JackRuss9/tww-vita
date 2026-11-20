# Progreso de Wind Waker nativo en PC

Objetivo: arrancar por el menú original, elegir o crear una partida, introducir
el nombre, reproducir la introducción y completar Isla Initia hasta embarcar.
La versión de PS Vita queda aplazada hasta conseguirlo en PC.

**Objetivo de PC alcanzado: Isla Initia completa en una sola partida
(`probe-4yK0MI`, resultado 0).** Partida nueva desde el título, jugada por el
usuario, sin restaurar estados ni inyectar entradas, hasta controlar a Link en
el barco pirata tras la despedida. Quedan defectos de presentación y audio
anotados más abajo (música de fondo, fanfarrias y voces; parpadeos en la
despedida); no impiden avanzar.

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
| Inicio directo de diagnóstico en Isla Initia | Link camina, gira, se detiene, rueda y sigue la altura del terreno | Usa `--outset`: salta menú e introducción y omite la escena de entrada; permite los eventos posteriores |
| Agua en la isla | Se alcanzan estados de natación y reaparece en la isla | No es una prueba de progreso de la historia |
| Cámara | Seguimiento, recentrado con L y cámara orbital PC con C-stick | Límites por zona; no reproduce toda la cámara manual original ni sus variantes especiales |
| Ropajes, catalejo y cinemática posterior | Superada la secuencia aislada: tutorial, `zelda_fly`, conversación posterior y movimiento | Solicitudes de escenario para omitir desplazamientos, asignación del objeto a X y mando simulado; sin ventana ni sonido; no valida todo el recorrido desde el menú |
| Estabilidad del movimiento | Pruebas de 120 y 75 segundos tras corregir el temporizador y las animaciones | No valida todas las zonas ni interacciones |
| Arranque normal, nombre, historia y despertar | Superada la secuencia completa y entregado el control; Link recorre 166,342 unidades con entrada de teclado | Diagnóstico sin ventana y con audio mudo; no valida las tareas posteriores |
| Salida de audio real | SDL abre a 32 kHz; la grabación contiene señal sin saturación en el tramo medido | Se han observado interrupciones; faltan funciones de música y efectos |
| Pruebas numéricas de temporizador y animación | Superadas | Casos de cálculo, no prueba de juego completo |
| Regresión del mezclador DSP | Superada: retornos, filtros, realimentación, liberación y osciladores | No comprueba fidelidad de Wind Waker frente a la consola |
| Aplicación de parches en fuentes limpias | Superada: 71 de tww y 6 de sms-port | No reemplaza los árboles locales existentes |
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

En la repetición posterior, el usuario confirma que el efecto de la voltereta
ya está sincronizado. Las voces de Link y los tirones de las olas siguen
pendientes; no se atribuye esa mejora de sincronización a una causa medida.

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

El usuario confirma en esa ejecución visible que ya puede entrar en casa.
Al hablar con la abuela se llega a mostrar los ropajes del héroe, pero aparece
un **segundo cierre**, distinto: la comprobación `groupID < 16` en
`JPAEmitterManager::createSimpleEmitterID`. La traza viene del adaptador de
partículas JStudio. `adaptor_do_PARTICLE` aún leía el selector empaquetado
como entero nativo, aunque el contenido STB permanece big-endian. El efecto
`0x000001fa` se interpretaba como `0xfa010000`, grupo 250. El parche 0045
usa la lectura big-endian para recuperar grupo 0, recurso 0 y efecto 506.

`probe-eXPiWb` completa la escena aislada de los ropajes, resultado 0: comprueba
que el selector leído coincide con sus bytes originales, crea el efecto en
el grupo válido, deja a Link vestido de héroe y verifica 118,535 unidades de
movimiento con teclado después de recuperar el control. No modifica las
banderas de ropa: el cambio lo realiza la escena del juego. Parte de una
solicitud de escenario `LinkRM`, punto 200, habitación 0, capa 8; **no valida
todo el recorrido desde el menú ni la entrega del catalejo**.

`tools/clothes_scene.gdb` permite repetir esa regresión. El primer intento
`probe-F0m5M1` se detuvo por argumentos optimizados fuera del alcance del
observador de GDB; se ajustó el punto de observación al inicio de la función
compilada. No se cuenta ese intento como cierre nativo del juego.
La compilación PC y la aplicación limpia de los 45 parches pasan.

`probe-UAs7rS` repite esa misma escena **con ventana, sonido real y reloj
normal**, y termina también con resultado 0, ropa de héroe y 118,535 unidades
de movimiento después de la escena. Registra una interrupción de audio de
54 ms durante el ensayo con depurador; no valida reproducción sin cortes.
La ventana se cierra al alcanzar la condición de éxito del ensayo automático.

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

# Escena aislada de los ropajes, partículas y retorno del control:
./tools/probe-pc.sh --outset --headless --deterministic --seconds 240 \
  --gdb "$PWD/tools/clothes_scene.gdb"
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

## Cámaras de Abril y entrega del catalejo (15 de noviembre)

El parche 0046 elimina una restricción demasiado amplia de `--outset`:
rechazaba todas las órdenes de eventos en la habitación 44, incluidas las de
Abril después de los ropajes. Se conserva únicamente la omisión de la escena
de entrada de ese modo de diagnóstico. En este punto de la historia, Abril
inicia automáticamente `omedeto` al acercarse; no necesita el indicador Hablar.
`probe-f4966f` comprueba que el inicio directo sigue entregando control y
movimiento (238,604 unidades, resultado 0).

El parche 0047 añade `FIXEDFRM` relativo a `@STARTER` con máscara `oo`:
transforma los desplazamientos originales usando la atención y orientación
del actor. Los tres planos de `omedeto` se comparan con una rotación calculada
independientemente; el error máximo medido es 0,141 unidades. Otros actores
relativos y máscaras siguen pendientes. Una primera comprobación numérica
detectó una modificación incorrecta de los accesores angulares; se retiró.

El parche 0048 recupera `GETITEM` desde el ensamblador original: espera de
27 fotogramas, selección de cámara con colisiones, transición de 5 fotogramas
y señal de preparado. El catalejo entra en la ranura 0 con ID `0x20`. La prueba
anterior `probe-snn1vG` llegaba aquí pero se detenía en el diálogo posterior:
`TALK` seguía vacío. Esa prueba no se considera superada.

El parche 0049 recupera el envoltorio de evento `TALK` y añade una cámara PC
funcional para el plano normal (comando de mensaje 0): usa los actores reales,
los parámetros del estilo, un encuadre para ambos interlocutores y una
transición de 20 fotogramas antes de dar la señal al sistema de mensajes.
**No es una reconstrucción exacta de toda la cámara original.** Los comandos
especiales de cámara de mensaje aún se rechazan expresamente. No marca
objetos ni diálogos como completados para saltarlos.

`probe-b4vlMv` supera ropajes, felicitación, entrega del catalejo, conversación
posterior y devolución del control: movimiento final de 234,527 unidades,
resultado 0. La captura de conversación detectó un poste tapando a Link;
se añadió una búsqueda de ángulos con líneas de visión de ambos actores.
`probe-sRpzuV` repite la secuencia con esa mejora: resultado 0, ambos
personajes visibles en la captura y movimiento final de 234,549 unidades.
El observador comprueba que las señales de preparado permanecen apagadas
antes del fotograma 20, que se activan al completar el encuadre y que el centro
y campo de visión llegan a sus valores objetivo. `probe-LQEGlr` fue un fallo
del nombre del símbolo en el observador GDB, corregido antes de esta prueba;
no fue un cierre nativo del juego.

`probe-QXA2ta` comprueba también el arranque normal con la compilación final:
menú, nueva partida y nombre, historia, despertar en la torre y devolución del
control, resultado 0 y movimiento de 166,342 unidades. Sin ventana y con audio
mudo; no es una nueva comprobación auditiva ni una prueba hasta el embarque.
La aplicación limpia de los 49 parches reproduce los dos archivos de cámara
usados en esta compilación.

Regresión reproducible:

```sh
./tools/probe-pc.sh --outset --headless --deterministic --seconds 600 \
  --gdb "$PWD/tools/aryll_story.gdb"
```

Esta prueba usa solicitudes de escenario para entrar en la escena de los
ropajes y volver a la torre, y teclado para avanzar mensajes y caminar.
Conserva las banderas e inventario obtenidos por los eventos reales; **no
valida el recorrido completo desde el menú hasta Abril**. `tools/normal_flow.gdb`
sigue siendo la prueba independiente del arranque normal.

En esta comprobación todavía no estaba disponible la cámara libre (ver el
avance 0050 a continuación). También siguen pendientes la música
de fondo, la fanfarria de objeto, las voces de Link y el recorrido posterior
con el catalejo, espada, bosque y embarque. El usuario confirma que los sonidos
de menú y diálogo, la sincronización de la voltereta y el parpadeo de la torre
han mejorado; esas observaciones no equivalen a fidelidad de todo el audio.

## Cámara libre en PC (parche 0050)

La entrada del C-stick ya llegaba al juego, pero `manualCamera` devolvía sin
actualizar la vista. El parche 0050 añade una cámara orbital PC alrededor de
Link: giro horizontal, inclinación acotada por el estilo de cada zona,
seguimiento al caminar y conservación de la distancia al entrar. Reutiliza
la selección de modos, el recentrado con L y las colisiones del juego.

**Es una implementación funcional PC, no una reconstrucción exacta de toda
la función original.** La entrada vertical inclina la vista; todavía no
recupera su acoplamiento original con zoom/campo de visión ni todos los casos
de combate, navegación y otros actores. No se da por validada en todo el juego.

Controles: **I/J/K/L** equivalen al C-stick; **Q** al botón L de recentrado.
El stick derecho de un mando usa la misma entrada y lógica, aunque las pruebas
automáticas siguientes utilizan teclado.

`probe-IPp5jo` supera giro a ambos lados, inclinación, más de dos vueltas
completas sin discontinuidades numéricas, ausencia de deriva al soltar,
movimiento de Link y vuelta al seguimiento con L en la playa (resultado 0).
`probe-Ae4oIf` detecta un error del primer intento en la casa: el estilo MM18
guarda los límites como 35/0, mientras que MM01 usa 0/30. Se corrigió la lectura
de ambos extremos; el fallo fue una aserción del observador, no un cierre nativo.
`probe-nZDFXv` supera exterior e interior con esa corrección (resultado 0),
con inclinaciones máximas de 29,998° y 34,997° respectivamente. Girar la cámara
no desplaza a Link y al soltar el C-stick no se mide deriva horizontal.

La inspección de capturas encuentra que la pared puede acercar la vista al
modelo de Link. Se habilita el mecanismo común de ocultación de cabeza/cuerpo
cuando la distancia cae por debajo del umbral original, sin cambiar el modelo
ni las colisiones. `probe-kFU55x` repite la prueba completa **con ventana y reloj normal** en la
compilación final, resultado 0. Comprueba 100 fotogramas de vista cercana con
la señal de ocultación activa y su retirada al volver al seguimiento con L.
Las capturas permiten revisar el resultado; no se ha validado cada ángulo
posible dentro de habitaciones estrechas.

```sh
./tools/probe-pc.sh --outset --headless --deterministic --seconds 180 \
  --gdb "$PWD/tools/manual_camera.gdb"
```

La prueba entra en `LinkRM` mediante una solicitud de escenario; no valida
la interacción con la puerta ni avanza la historia. Audio silenciado.

`probe-VX57Ad` gira la cámara manual antes de acercarse a Abril y supera
ropajes, felicitación, entrega del catalejo, diálogo y movimiento final de
234,579 unidades (resultado 0). Comprueba el relevo entre cámara manual y
cámaras de eventos; precede al último ajuste de visibilidad del modelo.
`tools/aryll_story.gdb` incluye esa entrada de C-stick para repetirlo.
`probe-Pkl2hc` amplía la regresión final con movimiento y giro simultáneos,
también en ambas zonas: resultado 0. Link se desplaza mientras la cámara gira
471,885° en cada tramo; se comprueban además 111 fotogramas de ocultación en
vista cercana. La serie de 50 parches se aplica sobre fuentes limpias y reproduce
el archivo de cámara usado para compilar.


## Regresión de I, reaparición y parpadeo en ventana (0051 / sms-port 0005)

La prueba manual desde el menú detectó fallos que las pruebas anteriores no
cubrían: parpadeo en título y primera conversación, cortes en la música del
vídeo, pantalla negra después de I y pérdida de movimiento tras reaparecer.
Por tanto, los ensayos de 0050 **no demostraban que la experiencia completa
fuera correcta**.

`probe-MbsFJx` reproduce el primer fallo de cámara: pulsar I antes de girar a
los lados entra en la vista subjetiva, estilo SX31. Su transición ordinaria
nunca activaba las señales de preparado, porque estaban dentro de la rama
reservada al catalejo. Al agotarse los siete fotogramas, dividía por cero y
corrompía la vista. El parche 0051 recupera el cierre común de la transición,
acota el divisor y corrige las condiciones de objetivo y las actualizaciones
angulares contrastadas con el ensamblador original. `probe-e7ba1F` completa
esa vista y sale con Q, resultado 0.

Además, en PC todas las direcciones del C-stick entran ahora en la cámara
orbital: I/arriba ya no solicita primero la vista subjetiva. `probe-bN1DwK`
comprueba esa entrada y el recentrado, resultado 0. `probe-A76HLF` reproduce
I primero, I nadando, agotamiento natural del temporizador, reaparición y
movimiento posterior de 306,431 unidades, con cámara válida (resultado 0).
No modifica posiciones, temporizadores ni banderas de historia. El intento
anterior `probe-u940tc` llegó a reaparecer y moverse, pero su observador
esperaba seguir en tierra mientras la entrada automática volvía al agua; se
interrumpió para corregir la comprobación y no se cuenta como superado.

```sh
./tools/probe-pc.sh --outset --headless --deterministic --seconds 180 \
  --gdb "$PWD/tools/camera_recovery.gdb"
```

El parpadeo se pudo capturar directamente en la ventana X11 de la partida:
alternaba una escena completa con una imagen mayormente negra, dibujada
solo en una esquina. Las capturas internas sin ventana del título
(`probe-8iG3Hj`, `probe-B2oLYx`) no mostraban esa alternancia; ambos ensayos
terminan por su límite de tiempo, no son pruebas de historia superadas.

El parche sms-port 0005 restaura el framebuffer del juego **dentro** de la
misma orden de presentación en el hilo GL, inmediatamente después del swap.
La primera revisión mejoró las capturas de la playa (`probe-JTmT0c`), pero el
usuario volvió a observar parpadeos en el menú: **no bastaba para cerrar el fallo**.
La revisión actual vacía primero la cola GL manteniendo el servicio de audio y
protege la presentación breve frente a ejecución reentrante del juego. Las
otras esperas siguen atendiendo las interrupciones de audio.

La prueba de la función de presentación real comprueba la restauración antes
del siguiente dibujo, la exclusión de ejecución reentrante, SDL, HDR y ventana
minimizada. Falla con la implementación anterior y pasa con la actual:

```sh
python3 check/test_window_present.py
```

`probe-Eeu9DA` alcanza título y selección de fichero y termina con resultado 0.
Las 19 capturas válidas del selector muestran imágenes completas, sin la
alternancia negra anterior. El capturador intentó seguir después de que la
prueba cerrara su ventana y recibió XBadDrawable; no fue un cierre inesperado
del juego. **Falta confirmar el título y las conversaciones jugando normalmente**;
estas muestras no demuestran la ausencia del defecto durante toda la partida.

A petición del usuario, **las flechas controlan la cámara y WASD mueve a
Link y navega los menús**. IJKL sigue como alternativa; Q recentra. La
configuración está en `config/pc-bindings.txt`, respetando un archivo local o
`SMS_BINDINGS` explícito. `probe-c8fJRc` repite exterior/interior con las flechas,
movimiento y giro simultáneos, sin desplazar a Link al girar en reposo y con
retorno mediante Q (resultado 0).

Compilación y aplicación limpia: 52 parches de tww y 5 de sms-port; se
reproducen byte a byte la cámara, Link y los tres archivos de presentación GL
utilizados para compilar. Los cambios locales anteriores se conservan.

## Cierre al sacar el telescopio (0052)

El registro de la partida del usuario aborta al crear el modelo del catalejo:
la reserva solicitada es de 0x108 bytes y el heap reutilizable tiene capacidad
cero. Una reserva provisional sin uso puede ser eliminada por el compilador;
reducir después el heap lo dejaba vacío. El parche 0052 conserva en PC la
capacidad reservada de los dos heaps de equipamiento.

`probe-blwa07` termina con resultado 0: obtiene ropajes y catalejo mediante
los eventos, comprueba ambas capacidades (0xe610), equipa el objeto obtenido,
crea su modelo y mantiene una vista finita durante la apertura. Es una prueba
aislada: usa solicitudes de escenario y asigna el objeto a X directamente;
**no valida la interfaz de asignación ni completa el tutorial del cartero**.

```sh
TWW_TEST_TELESCOPE_MODEL=1 ./tools/probe-pc.sh --outset --headless \
  --deterministic --seconds 600 --gdb "$PWD/tools/aryll_story.gdb"
```

Los intentos automáticos de completar el tutorial todavía no lo han superado.
La secuencia exige apuntar al cartero y ampliar, y después mirar al cielo;
que el controlador de prueba no la complete no demuestra por sí solo otro
fallo del juego. El NPC ausente que pide enfocar con L corresponde a Sturgeon
(`d_a_npc_aj1.cpp`): su creación y lógica están vacías en esta dependencia y
su perfil está excluido del ejecutable. Ese caso requiere implementar el actor.

## Cortes de sonido de la historia inicial: diagnóstico abierto

El usuario confirma que son cortes de sonido. La historia ilustrada reproduce
`Audiores/Stream/title.afc`; no es una película THP. En ocho segundos analizados,
el decodificador mantiene más de un segundo de datos disponibles. Una lectura
y decodificación independiente del archivo del disco coincide exactamente con
409600 muestras por canal del decodificador en ejecución (cero diferencias).
La grabación interna mezclada conserva el avance musical, con un desfase
estable de aproximadamente 15,61 segundos respecto al inicio del WAV de arranque.

Esto comprueba el tramo del decodificador y la mezcla, **no la continuidad de
la salida que escucha el usuario**. Las 60 estadísticas del registro original
indican cero underruns SDL. En cambio, el ensayo instrumentado sí registra
interrupciones: GDB detiene también el hilo de sonido, por lo que esas cifras
no sirven para atribuir el fallo original. La verificación final debe hacerse
sin depurador durante la reproducción.

Una segunda medición arranca con teclado desde título y tarjeta aislada,
desconecta GDB al comenzar la música y deja reproducir 30 segundos con salida
SDL real, sin capturas. Las seis muestras posteriores informan cero underruns,
con cola de 56–69 ms. El cierre inicial del grupo de procesos no terminó el juego desconectado de
GDB: quedó sonando sin ventana. Se cerró después de que el usuario lo comunicara,
se comprobó que no quedaban procesos y se corrigió el cierre del controlador.
No es una validación auditiva ni mide el transporte del sistema hasta
los altavoces: **los cortes comunicados por el usuario siguen abiertos**.

## Avance del tutorial y conversación posterior (0053 / 0054)

El sonido queda en segundo plano por indicación del usuario: en su última
prueba le parecía correcto. La prioridad vuelve al progreso de la historia.
Todos los ensayos nuevos de este apartado desactivan la salida de sonido.

El parche 0053 corrige cálculos de cámara subjetiva contrastados con el
original: lee la inclinación del cuerpo de Link (0x2B4, instrucción 80171128),
y separa inclinación y giro horizontal al fijar un objetivo (8017144C y
80171578). No modifica banderas para superar el tutorial.

`probe-KpMk8u` llega mediante entrada de mando al enfoque del cartero, zoom,
reacción y orden real de `zelda_fly`. Después encuentra un bloqueo nuevo:
la conversación solicita TALK 12 y la implementación PC solo aceptaba TALK 0;
por eso la cámara nunca indicaba que estaba lista. El ensayo se interrumpió
para implementar esa variante, sin contar como prueba completa superada.

El parche 0054 implementa la familia de encuadres sobre el hombro TALK
11/12/30/31 a partir de 8016F978..8016FCC8: posición relativa de los dos
interlocutores, inversión de primer plano/objetivo, offsets de altura de los
ojos, FOV 55/65 y disponibilidad inmediata del encuadre. Otras variantes
especiales de conversación siguen sin estar implementadas. El uso real de
TALK 12 queda comprobado; las otras tres comparten implementación pero no
se han recorrido en sus conversaciones del juego.

**`probe-5fUimA`, resultado 0:** obtiene los ropajes y el catalejo por sus
eventos, abre el modelo, recorre las cuatro fases del tutorial, ordena y
reproduce `zelda_fly`, supera la conversación posterior con TALK 12 listo y
FOV 55, recupera el control y camina **234,382 unidades**. Se comprueban vistas
finitas; no se fuerza la disponibilidad de la cámara ni el fin de los eventos.

```sh
SMS_AUDIO=0 SMS_AUDIO_OUT=null SDL_AUDIODRIVER=dummy \
  TWW_TEST_TELESCOPE_STORY=1 ./tools/probe-pc.sh --outset --headless \
  --deterministic --no-captures --seconds 900 --gdb "$PWD/tools/aryll_story.gdb"
```

El controlador reproducible está en `tools/telescope_story.py`. Usa ejes
nativos de mando para apuntar, C-stick para ampliar y botones para pasar
mensajes. El padre sigue usando solicitudes de escenario para omitir los
desplazamientos hasta la abuela y Abril, y asigna el objeto obtenido a X.
**No es una partida completa desde el menú, ni una comprobación visual o de
sonido. El apuntado con teclado aún no está validado**: los controladores
digitales anteriores oscilaban alrededor del objetivo. El port ya admite
Ctrl izquierdo + WASD para media inclinación del stick; se documenta, sin
presentarlo como una solución del tutorial comprobada con teclado.

Los ensayos `probe-BdLGzo` (error del observador), `probe-A7jD2v` y
`probe-Oq922E` (controladores incompletos), y `probe-gnQLD2` (detenido al atender
la petición de silencio) no cuentan como pruebas superadas. Se conservaron
las partidas del usuario. Compilación PC y aplicación limpia de **54 parches
tww / 5 sms-port** pasan; el árbol limpio reproduce la cámara compilada.

## Punto de parada anterior, 16 de noviembre: Orca (0055)

**Estado histórico del 16 de noviembre; superado por las pruebas del 18 de noviembre descritas abajo.** En ese momento no se había conseguido aún la espada.
La última ejecución se cerró de forma deliberada; el resultado 255 de ese
ensayo no representa un cierre inesperado del juego.

- La casa `Ojhous` carga con Orca y sus objetos. El ensayo inicial `probe-vpQ1Lc`
  partía del estado anterior al catalejo: servía para comprobar carga y diálogo,
  no la petición de espada posterior a la cinemática.
- El parche 0055 resuelve FIXEDFRM `@PARTNER/oo` mediante el **segundo
  participante del evento** (`getPt2`), siguiendo 800B0858..800B0874. Es un
  identificador diferente del interlocutor de mensajes. La entrada usa la
  puerta como participante; no se sustituye por Orca ni por Link.
- `probe-cvG47g` repite con éxito el tutorial, `zelda_fly`, la conversación y
  movimiento (234,382 unidades), y continúa a Orca conservando ese estado.
  Se detiene por una suposición incorrecta del observador de participantes,
  posteriormente corregida; no se cuenta como entrenamiento superado.
- `probe-flFuZa` y `probe-gImrnP` reanudan una copia local del estado realmente
  obtenido en `cvG47g`. Comprueban numéricamente Eye/Center y disponibilidad
  de la cámara PARTNER, navegación dentro de la casa, diálogo 0x965 → 0x966
  → 0x94e y aceptación de la propuesta de entrenamiento. Esta reanudación
  diagnóstica no demuestra el recorrido completo desde el menú ni la puerta.
- **Bloqueo identificado:** evento `Ji1_linkmove`, cámara `UNITRANS`, corte 24.
  Al detener la prueba, Link, Orca, director/fundido y temporizador habían
  terminado sus partes; la cámara seguía sin completar su bandera 52 y con
  disponibilidad 0/0/0. `uniformTransEvCamera()` todavía está vacía en la
  dependencia. No se ha forzado esa bandera para continuar.

Lo siguiente es implementar y comprobar la transición UNITRANS que pide
este evento: 60 fotogramas, posiciones iniciales/finales explícitas, FOV 51,
`BSpCurve=0`, `BGCheck=0`. Después se podrá probar el entrenamiento real
(cortes horizontal/vertical, estocada, giro, contraataque y salto) y la entrega
de la espada. **Ni esas lecciones ni la espada se consideran validadas.**

El controlador experimental está en `tools/orca_story.py`, activado con
`TWW_TEST_ORCA=1` en el recorrido de Abril. Su lógica de golpes todavía no
ha podido probarse por ese bloqueo. Todas las ejecuciones de este apartado
usan salida de sonido desactivada y tarjetas separadas. La compilación y la
aplicación limpia de 55 parches tww / 5 sms-port pasan y reproducen ambos
archivos de cámara. Los registros, prototipos y la copia de estado se conservan
en `run-pc/orca-regression/` y `run-pc/probe-*/`, sin subir datos de juego.

## 18 de noviembre: entrenamiento de Orca y espada (0056 / 0057)

**La prueba `probe-IKgLHt` termina con resultado 0:** completa las seis lecciones
con entradas normales, obtiene la espada por la presentación real, comprueba
su registro en el inventario (`0x38`), termina la conversación y recupera el
control. Link camina **236,439 unidades** después de la entrega.

Son comprobaciones de lógica en PC sin ventana ni sonido. Se reanuda el estado
obtenido anteriormente tras el catalejo y se solicita el escenario `Ojhous`;
**no demuestran el trayecto desde el menú, la puerta ni la fidelidad visual**.
No se modifican contadores de golpes, banderas de historia ni inventario para
superar el entrenamiento. Pasan cortes horizontal/vertical, estocada, giro,
contraataque y salto, así como la recuperación tras golpes rechazados por Orca.

### Problemas encontrados y correcciones

- **UNITRANS vacía:** bloqueaba `Ji1_linkmove` antes de la primera lección.
  El parche 0056 recupera la variante cartesiana absoluta lineal. La prueba
  comprueba Eye, Center, FOV, disponibilidad y retorno en todos los fotogramas,
  con muestras registradas al inicio, mitad, destino y final (60 y 30 frames).
  Se conserva la regla original `(frame+1)/Timer`, con fin en `frame>=Timer`;
  no se fuerza la bandera del evento. Curvas, referencias a actores, corrección
  de colisión y `Cushion` distinto de 1 siguen explícitamente sin implementar
  para esta función.
- **FIXEDPOS vacía:** en `Ji1_BadSpeak` impedía abrir la corrección de un golpe
  incorrecto (mensaje 0x96f en el primer ensayo). El parche 0057 fija Eye al
  empezar el encuadre y sigue la atención de Link con `CtrCus`. Se comprueban
  numéricamente la posición relativa, Center suavizado y disponibilidad al
  abrir las correcciones reales. Otros actores como objetivo siguen pendientes;
  no se presenta como reconstrucción completa de todas las variantes.
- **Controlador de prueba:** atacaba durante la aproximación y provocaba
  golpes distintos del solicitado. Se espera a estar cerca para los primeros
  cortes. Otro error leía `mAStatus` en vez de `mDoStatus` para el aviso de
  contraataque; se corrigió el controlador, sin cambiar el juego para aceptarlo.
- **Diagnóstico:** los índices sin datos pueden ser `0xffffffff`. Se acotaron
  sus recorridos a la tabla de sustancias para no mostrar lecturas ajenas al
  corte. Es un fallo del observador, no una corrupción demostrada del juego.

`probe-sOYoKv` demuestra UNITRANS y descubre FIXEDPOS, pero queda incompleto
(sin archivo final de estado tras la interrupción del servidor); no es un pase.
`probe-bwWCN6` avanza hasta contraataque con la lectura de interfaz incorrecta;
termina por el límite externo con estado 137 durante el ajuste del controlador,
no se cuenta como entrenamiento superado. La prueba aprobada es `probe-IKgLHt`,
repetida desde el inicio del ensayo con las correcciones del controlador.

El punto de reanudación explícito se maneja con `tools/orca_replay.gdb`:

```sh
SMS_AUDIO=0 SMS_AUDIO_OUT=null SDL_AUDIODRIVER=dummy \
  TWW_ORCA_CHECKPOINT="$PWD/run-pc/orca-regression/earned-savedata.bin" \
  ./tools/probe-pc.sh --outset --headless --deterministic --no-captures \
  --seconds 900 --gdb "$PWD/tools/orca_replay.gdb"
```

El archivo de estado es local y no se distribuye: se obtuvo con el recorrido
real de eventos, y puede generarse mediante `TWW_TEST_ORCA=1` y
`TWW_ORCA_SAVE_CHECKPOINT=/ruta/local` en `tools/aryll_story.gdb`. No se usa una
partida del usuario. Compilación PC y aplicación limpia de **57 parches tww /
5 sms-port** pasan y reproducen ambas cámaras y el código de Link.

**Siguiente investigación en curso:** repetir la obtención para conservar un
punto con la espada ganada y comprobar el bosque `A_mori`. El controlador
experimental `tools/forest_story.py` todavía no demuestra el rescate de Tetra,
la salida del bosque ni el embarque. La primera ejecución encadenada es
`probe-DOJypj`; sus resultados se anotarán cuando se conozcan.

## Próximos hitos

- Confirmar la revisión del parpadeo en menú y conversaciones; comprobar
  también el movimiento de las olas. El usuario confirmó la sincronía al rodar.
- Confirmar visualmente el tutorial y su escena posterior en una partida
  normal, y comprobar la precisión de apuntado con teclado.
- Comprobar la entrada al bosque, combate y rescate de Tetra con la espada
  obtenida; después, regreso, escudo y despedida para embarcar.
- Corregir cámaras, diálogos y actores que bloqueen las tareas de la isla.
- Validar visualmente el entrenamiento y la espada; comprobar escudo, cambios
  de habitación y recorrido normal. El bosque y el embarque siguen pendientes.
- Audio en segundo plano: revisarlo si reaparecen fallos durante el recorrido;
  el usuario indica que actualmente le parece correcto.
- Publicar cada avance probado con comandos, resultado y fallos pendientes.

No hay todavía una estimación fiable de sesiones: faltan partes del código
original, además de errores de adaptación a PC. La estimación se revisará
cuando se complete el primer recorrido real y se identifiquen sus bloqueos.

## 18 de noviembre: entrada y cámaras del bosque (0058 / 0059)

El estado guardado local de diagnóstico procede de la espada ganada en las
seis lecciones de Orca. `tools/forest_replay.gdb` exige indicar explícitamente
`TWW_FOREST_CHECKPOINT`; restaura ese estado y solicita `A_mori`. No prueba el
recorrido desde el menú ni la puerta del bosque. Las pruebas se ejecutan sin
ventana, capturas ni salida de audio, conservando la tarjeta del usuario.

- `probe-DOJypj`: repite la espada y conserva el punto ganado; la presentación
  del bosque queda detenida en UNITRANS, cuya curva predeterminada faltaba.
- 0058 recupera la curva B-spline del original con claves 0,0,1,1 y Cushion.
  Las transiciones de 70 y 20 fotogramas del bosque superan comprobaciones
  independientes de posición, FOV, disponibilidad y finalización por fotograma.
  `probe-t43LWK` termina con resultado 0 y conserva también las transiciones
  lineales de 60 y 30 fotogramas de Orca; este modo no repite las seis lecciones.
- `probe-jnxL8V`: termina la presentación, pero el controlador persigue enemigos
  aún suspendidos en el aire. No se demuestra un fallo del enemigo.
- `probe-0QSOGQ`: priorizar al enemigo del suelo evita lo anterior, pero la ruta
  directa choca con un desnivel. Es un fallo de navegación de la prueba.
- `probe-ow9Aej`: una ruta local de 17 puntos, recorrida con entradas de mando,
  alcanza la zona del primer enemigo sin trasladar artificialmente a Link.
  Se bloquea en AMORI_DEMO_2, corte 34 RESTOREPOS, sin argumentos. Los otros
  participantes han terminado; la función de cámara todavía estaba vacía.
  Finaliza por el límite de tiempo; no es un cierre espontáneo del juego.
- 0059 implementa la restauración de vistas guardadas, selección según distancia,
  visibilidad y obstáculos, y transición FarTimer del original. Dest 9 y objetivos
  distintos de @PLAYER siguen explícitamente pendientes. Compilación PC superada;
  `probe-DJRkq4` supera la restauración (modo 2, 30 fotogramas), devuelve el
  control y derrota al primer Bokoblin con ataques normales: vida 0 y eliminación
  posterior del actor. No hay combate ni rescate completo: el controlador se
  atasca contra el desnivel al perseguir los dos enemigos transportados.

Reproducción desde fuentes limpias: **59 parches tww** aplicados y coincidencia
exacta de los dos archivos de cámara y del archivo principal de Link. El combate,
rescate de Tetra, regreso, escudo y embarque siguen sin estar validados.
Las rutas y partidas locales contienen datos propios del juego y no se publican.

**Regresión cerrada `probe-orpEW2`: resultado 0.** Con
`TWW_FOREST_FIRST_FIGHT=1` verifica las curvas de la introducción, la ruta,
RESTOREPOS con vista finita y disponibilidad, control recuperado y primer
Bokoblin derrotado mediante ataques. No equivale al rescate de Tetra.
La prueba anterior DJRkq4 termina por límite de tiempo (124); su diagnóstico
final de GDB falló porque se editó el archivo de órdenes mientras estaba
abierto. Los resultados previos de juego quedan en el registro; la repetición
cerrada evita depender de ese diagnóstico.

Repetición del hito (requiere los puntos locales ganados y la ruta local):

```sh
SMS_AUDIO=0 SMS_AUDIO_OUT=null SDL_AUDIODRIVER=dummy \
TWW_FOREST_FIRST_FIGHT=1 \
TWW_FOREST_CHECKPOINT="$PWD/run-pc/orca-regression/earned-sword-savedata.bin" \
TWW_FOREST_ROUTE="$PWD/run-pc/orca-regression/forest-path.json" \
./tools/probe-pc.sh --outset --headless --deterministic --no-captures \
  --seconds 240 --gdb "$PWD/tools/forest_replay.gdb"
```

Los pájaros tienen una condición de activación por interruptor de escenario;
no deben perseguirse mientras esperan. La siguiente prueba, `probe-FnFrxq`,
intenta llegar a esa zona con una ruta adicional de entradas reales, sin
activar el interruptor artificialmente. La navegación y el rescate permanecen
pendientes hasta conocer el resultado.

**Continuación `probe-FnFrxq`: límite de tiempo (124), sin cierre espontáneo.**
Repite el primer combate, pero el retorno propuesto intenta subir una pared de
unas 352 unidades y no lo consigue. Los pájaros siguen esperando la activación
del escenario; no se ha demostrado un fallo de su IA. `probe-KdgVt8` repite con
una ruta alternativa de escalones de hasta unas 245 unidades, todavía experimental.
También aparecen avisos de recursos `cut*_pos` sin conversión identificada;
no impiden la primera derrota, pero su fidelidad queda pendiente de revisar.

**Resultado de las rutas posteriores:** `probe-KdgVt8` y `probe-l0vHkS`
se detuvieron deliberadamente con SIGINT (resultado 255); no son fallos
espontáneos del juego. Ambos repiten la primera derrota, pero no alcanzan el
interruptor de los pájaros. La segunda prueba sustituye la alternancia de
teclas por dirección analógica continua: mejora el recorrido, pero tampoco
supera el borde. Diagnóstico: pared tipo 2, stick 1, alcance de agarre 170;
la ruta proponía una subida de unas 245 unidades. No se debe aumentar ese
alcance para dar la prueba por superada. Falta encontrar el recorrido normal
por escalones y saltos, incluyendo obstáculos que no recoge la malla estática.

**Estado al guardar:** ningún rescate ni embarque demostrado; ninguna prueba
queda ejecutándose. El primer combate tiene una regresión cerrada con resultado
0 (`probe-orpEW2`). La navegación posterior es experimental y conserva todos
sus fallos registrados. No se ha comprobado visualmente este tramo.

## Prueba manual: llegada al segundo encuentro del bosque (0060)

El usuario recorre el bosque, derrota al primer enemigo y alcanza los dos
siguientes. Esto demuestra que el atasco en los bordes era del controlador de
pruebas. Se reproduce un fallo real: Link invisible y sin control mientras los
enemigos atacan. El registro `forest-manual/before-target-fix.log` muestra
`unsupported FIXEDPOS target=@TARGET` y Link permanece en demo 2.

0060 añade la resolución de @TARGET al participante mPtItem del evento,
según getEvActor original 800B09B8..09DC. TagEv selecciona allí al Bokoblin
transportado al iniciar la escena. Compilación y reproducción limpia de los
60 parches superadas. **Validación del encuentro todavía pendiente.**

Los ensayos aislados LlsNRk, DeaUGt y elli8d no validan la escena: el primero
falla al leer `this` en una función optimizada (corregido con el argumento de
su envoltorio); los siguientes colocan a Link en un punto cuyo obstáculo lo
expulsa fuera del disparador. Son fallos del ensayo. Se retoma la prueba
manual con 0060 aplicada; el usuario conserva el control y su tarjeta habitual
sigue separada. La prueba manual anterior salió normalmente al cerrar la ventana.

## Prueba manual superada: Tetra rescatada y llegada a la playa (0060)

**0060 validada en juego por el usuario.** Desde el punto de diagnóstico del
bosque con la espada ganada, completa el primer combate, llega al encuentro
siguiente, derrota a ambos Bokoblins y rescata a Tetra. El usuario confirma
que después el pájaro secuestra a Abril y se llega a la conversación con Tetra
en la playa, delante del barco. El registro confirma vida 0 en los enemigos
508 y 509, salida de A_mori y entrada en sea. La corrección FIXEDPOS @TARGET
permite superar el bloqueo anterior sin saltar el evento ni modificar vidas.

No se observó toda la partida en vídeo continuo; el recorrido y las escenas
se apoyan en la confirmación del usuario y en el registro. Se capturó el nuevo
bloqueo en la playa: primer plano de Tetra, sin diálogo activo y Link en demo 2.
La tarjeta habitual del usuario sigue separada de esta prueba.

**Nuevo bloqueo confirmado:** evento `yuukaigo` (250), cámara corte 3366,
UNITRANS con Timer 10, TransType 1, RelActor @STARTER y RelUseMask --oo;
Center (55,-46,90), Eye (-137,30,228), Fovy 60, BGCheck 3. Esta variante
es relativa y no cartesiana, y el código actual la rechaza explícitamente.
Su flag de comienzo sí está activa; la cámara no termina y los participantes
esperan. No es todavía un fallo demostrado del diálogo de Tetra.

La ventana atascada se detuvo deliberadamente (SIGINT, salida 255) después
de capturar la imagen. Se conservan el registro, los datos del evento y un
volcado nativo del estado ganado en `run-pc/forest-manual/`. El volcado no
es un guardado completo de la escena: aún falta validar que permita repetir
el evento de la playa sin recorrer de nuevo el bosque. No se publican esos
archivos del juego. No queda la ventana ni su sonido activos.

Siguiente: recuperar la interpolación relativa de UNITRANS usada en yuukaigo,
comprobar la conversación y continuar hacia el escudo y el embarque. No se
ha demostrado aún la salida de la isla ni un recorrido completo desde el menú.

## Conversación de la playa superada en prueba automática (0061)

**`probe-l0u3Tn`: resultado 0.** Desde el estado realmente ganado al rescatar
a Tetra y llegar a la playa, el evento `yuukaigo` termina de forma natural,
Link recupera el control y recorre **73,709 unidades** con entrada de teclado.
No se fuerzan finales de cortes, banderas de misión ni inventario. El ensayo
recarga sea desde el punto de diagnóstico: no repite menú, bosque ni secuestro.
Falta la confirmación visual y sonora de este tramo por el usuario.

0061 recupera dos variantes compartidas:

- UNITRANS cartesiana o esférica con extremos relativos a los participantes
  @PLAYER, @STARTER, @PARTNER y @TARGET. La máscara de cuatro caracteres
  distingue coordenadas del mundo (-) y del actor (o); los extremos relativos
  se resuelven en cada fotograma. Se conserva la temporización B-spline/lineal,
  Cushion y la interpolación angular corta del código original.
- FIXEDFRM @TARGET enfoca al participante mPtItem, necesario cuando el cartero
  interviene en la conversación. Otros actores por nombre, máscaras especiales
  y TransType 2 siguen pendientes de implementar.

`tools/beach_camera.py` comprueba por fotograma la geometría con un cálculo
independiente: rotación y punto de atención de Tetra, centros, órbita esférica,
FOV, disponibilidad y finalización. Superadas las transiciones de **10 y 20
fotogramas**. La tolerancia de posición contempla la cuantización de ángulos
del juego y las coordenadas grandes del mundo, no valida toda la imagen.

Regresión `probe-vjMaJQ`: resultado 0; las transiciones lineales de 60/30
fotogramas de Orca y el comienzo del entrenamiento siguen funcionando. Ese
modo no repite las seis lecciones de espada. Compilación PC y reproducción
limpia de **61 parches tww** superadas, con fuentes de cámara y Link idénticas.

Fallos y límites de los ensayos:

- `probe-diwziW`: parada de diagnóstico intencionada antes de ejecutar la
  transición; confirma que el volcado ganado permite reproducir yuukaigo.
- `probe-FmfLGo`: límite de tiempo 124; supera UNITRANS, pero descubre el
  siguiente FIXEDFRM @TARGET sin soporte en el corte 3369. Ambos se corrigen
  en 0061. No fue un cierre espontáneo del juego.
- `probe-0Rjcol`: verifica las dos transiciones numéricamente, pero termina
  interrumpida durante el cierre de GDB; no se cuenta como prueba completa.
- `probe-l0u3Tn`: repetición cerrada que demuestra evento terminado y movimiento.

Repetir el ensayo requiere el volcado local ganado (no incluido en GitHub):

```sh
SMS_AUDIO=0 SMS_AUDIO_OUT=null SDL_AUDIODRIVER=dummy \
TWW_BEACH_CHECKPOINT="$PWD/run-pc/forest-manual/beach-earned-savedata.bin" \
./tools/probe-pc.sh --outset --headless --deterministic --no-captures \
  --seconds 180 --gdb "$PWD/tools/beach_replay.gdb"
```

`TWW_BEACH_SAVE_CHECKPOINT` permite exportar el estado después de la prueba.
Siguiente hito: comprobar visualmente la playa, obtener el escudo y continuar
hacia la despedida y el embarque. La isla completa aún no está validada.

## Escudo obtenido; despedida bloqueada por actores sin implementar (0062)

**Prueba manual del usuario:** obtiene y equipa el escudo, vuelve a Tetra y
acepta subir al barco. La captura confirma el escudo y la colocación de Link,
piratas y abuela para la despedida. El registro confirma la petición nativa
`sea point 205 room 44 layer 10`. Se preservó el estado ganado en un volcado
local; no se incluyen partidas ni capturas del juego en el repositorio.

Se localizaron dos problemas sucesivos:

1. El atajo de diagnóstico `TWW_START_OUTSET` omitía cualquier demo inicial de
   sea/44, incluida la despedida. 0062 lo limita a los puntos de diagnóstico
   204 y 0. Las llegadas posteriores, como 205/capa 10, siguen el flujo original.
2. Al permitir ese arranque, el evento 115 `departure_DEMO` intenta reproducir
   `departure.stb` desde Demo05. El análisis del paquete acepta sus primeros
   objetos, pero rechaza el actor **Ym2**. `Ym1` y `Ym2` comparten
   `d_a_npc_ym1.cpp`, cuya creación, ejecución, dibujo y otras funciones están
   vacías en la decompilación actual. Esta cinemática necesita esos personajes;
   no se ha omitido su participación ni se ha saltado al siguiente escenario.

El paquete original solicita como siguiente destino **A_umikz, punto 204,
habitación 0**, pero no se ha alcanzado. Recuperar el código de estos NPC es
el siguiente trabajo concreto; podrían aparecer otros requisitos después.
El hito verificado llega ahora al escudo y a aceptar la salida, no a embarcar.

0062 también registra el nombre del actor de cinemática ausente y mantiene
pendiente un paquete que no carga. Evita conceder su EventFlag o dar la escena
por terminada cuando el reproductor devuelve un fallo. No corrige todavía la
creación de Ym1/Ym2.

Pruebas conservadas localmente:

- `probe-P6C2Oq`: límite de tiempo 124. Demuestra que el evento comienza tras
  restringir el atajo, pero el paquete falla y el código anterior lo termina
  sin reproducir la despedida.
- `probe-d9CnzW`: parada de diagnóstico intencionada (255). El reproductor
  acepta control, cámara, Link, barco, Tetra, abuela, piratas y otros objetos;
  falla exactamente al resolver el bloque JACT de Ym2.
- `probe-AlkjpH`: límite de tiempo 124. El mensaje de fallo y la escena pendiente
  aparecen con la protección; su primer observador de retorno no cerró la prueba.
- **`probe-hDQPUl`: resultado 0 del ensayo de fallo esperado.** Comprueba que
  la despedida arranca, el paquete fallido conserva su corte pendiente y la
  bandera de finalización 0x2401 sigue sin concederse. Este resultado prueba
  la protección, **no una despedida ni un embarque satisfactorios**.

Compilación PC y reproducción limpia de **62 parches tww** superadas; los
archivos de eventos, demo, cámara y Link coinciden con el árbol local. Todas
las pruebas automáticas son sin ventana ni sonido. La ventana manual atascada
se cerró expresamente con SIGINT; el juego no se cerró por un fallo espontáneo.

Para repetir la comprobación del bloqueo (necesita el volcado local ganado):

```sh
SMS_AUDIO=0 SMS_AUDIO_OUT=null SDL_AUDIODRIVER=dummy \
TWW_DEPARTURE_EXPECT_BLOCKER=1 \
TWW_DEPARTURE_CHECKPOINT="$PWD/run-pc/beach-regression/departure-earned-savedata.bin" \
./tools/probe-pc.sh --outset --headless --deterministic --no-captures \
  --seconds 90 --gdb "$PWD/tools/departure_replay.gdb"
```

También se observaron avisos UNITRANS `@PARTNER / --or` durante la prueba
manual. Esa máscara de cámara sigue pendiente, pero no se ha demostrado como
causa de la falta de reproducción de departure.stb. No confundir ambos problemas.

## Despedida y embarque completos desde el escudo ganado (0063)

**Resultado: superada la prueba automática `probe-H6LE1j` (resultado 0).**
Desde el volcado del estado ganado en la prueba manual (escudo y acuerdo con
Tetra), la petición nativa `sea 205 / habitación 44 / capa 10` inicia el evento
115 `departure_DEMO`; `departure.stb` se reproduce hasta su fotograma 2460
(fin natural, modo 2), concede la bandera 0x2401, carga **A_umikz** (punto 204)
y, tras el evento 46 de llegada, Link queda sin evento ni demo activos durante
60 fotogramas en la cubierta del barco pirata.

Las capturas (`probe-g3qvo0`, una cada 150 fotogramas) muestran la secuencia
real: barco, despedida de los isleños con «Good luck, Link!» (Mesa y Abe
incluidos), diálogo con Tetra en alta mar y la cubierta con el HUD.

Dos causas corregidas en 0063:

1. **Mesa (Ym1) y Abe (Ym2).** `d_a_npc_ym1.cpp` estaba vacío. Se ha
   reconstruido entero a partir del ensamblador retail GZLE01 (tipos según el
   parámetro y las banderas, modelos de cuerpo/cabeza/hoz, parpadeo, mensajes
   y banderas de conversación, siega de hierba, espera sentado de Abe y enlace
   con las cinemáticas), siguiendo la familia ya descompilada de Abril (ls1).
   Es una reconstrucción funcional, no una descompilación que coincida byte a
   byte. Fuera de la cinemática, su comportamiento interactivo en la isla no se
   ha probado todavía.
2. **Valores de función compuestos de JStudio.** Con los actores presentes el
   paquete se cargaba y fallaba (SIGSEGV en `composite_add`, `probe-z0APHy`).
   El puntero de función se guardaba con otro orden de parámetros; en
   PowerPC no importa, pero en x86 desordena la pila. En PC se llama ahora con
   su firma real. La regresión de la playa (`probe-9pxld4`) sigue superada.

Límites: la prueba restaura un estado ganado y solicita el destino nativo de
Tetra; no es todavía una partida continua desde el menú hasta el barco. Pulsa
A durante la demo para avanzar los diálogos. Sin ventana y con audio mudo.

```sh
SMS_AUDIO=0 SMS_AUDIO_OUT=null SDL_AUDIODRIVER=dummy \
TWW_DEPARTURE_CHECKPOINT="$PWD/run-pc/beach-regression/departure-earned-savedata.bin" \
./tools/probe-pc.sh --outset --headless --deterministic --no-captures \
  --seconds 200 --gdb "$PWD/tools/departure_replay.gdb"
```

El modo `TWW_DEPARTURE_EXPECT_BLOCKER` de la sección anterior ya no existe: el
bloqueo que comprobaba está resuelto. La protección de 0062 frente a paquetes
fallidos se mantiene.

## Recorrido continuo en curso, actores ausentes y apuntado fino (18 de noviembre)

**Recorrido continuo desde el menú: en curso, todavía sin resultado.** El
usuario juega una partida nueva bajo `tools/full_island.gdb`. El observador no
escribe memoria ni entradas: registra escenarios, eventos, equipo y
conversaciones con Mesa y Abe, y exporta el estado real en cada hito. La prueba
corta `probe-ironEw` confirma que Mesa (tipo 0) y Abe (tipo 2) se crean en la
isla en partida normal.

```sh
SMS_AUDIO=1 SMS_AUDIO_OUT=sdl SDL_AUDIODRIVER=pulseaudio SMS_GX_DUMP_EVERY=1800 \
./tools/probe-pc.sh --seconds 10800 --gdb "$PWD/tools/full_island.gdb"
# continuar una partida guardada de ese recorrido: añadir --save-dir <tarjeta>
```

**Actores de Isla Initia sin crear: no bloquean la historia.** Son Sue-Belle
(`yw1`), Joel y Zill (`ko1`), Rose (`ob1`) y Sturgeon (`aj1`). Se analizaron
todos los `.stb` del disco y las listas de eventos de la isla, el bosque, las
casas y el barco. Ninguna cinemática los usa. Solo aparecen en `Get_Rupee`
(Rose), `AJ_SPEAK` y los enfados de `Ojhous` (Sturgeon), eventos que inicia el
propio personaje. El mismo análisis encuentra a Mesa y Abe en `departure.stb`
y a Abril en sus dos eventos, como era de esperar.

**Apuntado con teclado (sms-port 0006).** El catalejo llega a la velocidad
máxima con el 70 % de inclinación del stick. Ctrl (media inclinación) deja el
valor en 0,93 y no reduce el giro. La nueva tecla Alt izquierdo
(`FINE_TILT`, ajustable con `SMS_FINE_TILT`) deja el stick en 0,46 y el giro
del catalejo en el 17,5 % del máximo, medido en el juego. Con ella Link no
llega a andar; es solo para apuntar. **El usuario supera con teclado el paso
del buzón** en la partida continua: Alt + WASD hasta centrar la cabeza del
cartero (no el buzón: Abril comprueba el punto de atención del cartero),
↑/`I` para el zoom y A para que Abril pase al siguiente mensaje; sin pulsar A,
la pregunta del buzón se queda en pantalla aunque el encuadre sea correcto.

**Mando bajo WSL2.** Linux solo recibe mandos mediante `usbipd-win`. El
receptor 8BitDo (`2dc8:310b`) en modo XInput expone el mando en una interfaz de
clase propietaria que necesita `xpad`, ausente en el kernel de WSL. Solo se
reconocen su teclado y ratón auxiliares, así que no es utilizable. No se ha
probado un mando en modo DirectInput o Switch.

## Expresiones faciales en las cinemáticas (0064)

El usuario observa que Tetra cae del cielo con los ojos abiertos. La causa era
general: `dDemo_actor_c::getP_BtpData`, `getP_BtkData` y `getP_BrkData`
estaban vacías, así que ninguna cinemática podía cambiar los ojos o la boca
(btp), el desplazamiento de textura de la cara (btk) ni los colores animados
(brk) de un personaje. 0064 las reconstruye a partir del original
(80069434..8006969C) y lee sus identificadores, que siguen en big-endian en PC.

- Despedida (`probe-p1pOFi`, resultado 0): Tetra recibe su btp y su btk del
  paquete. En el mismo fotograma, ahora guiña un ojo con gesto burlón donde
  antes tenía los ojos abiertos sin expresión. La prueba de la despedida sigue
  superada.
- Catalejo y `zelda_fly` (`probe-1GhtmF`, resultado 0): la secuencia completa
  sigue superada. En las capturas Tetra se ve demasiado pequeña para confirmar
  visualmente sus ojos durante la caída.

## Hito: Isla Initia completa desde el menú (18 de noviembre)

**`probe-4yK0MI`, resultado 0.** El observador pasivo `tools/full_island.gdb`
registra la partida del usuario (tarjeta nueva, título, unos 37 minutos de
juego, ninguna señal de fallo): despertar, ropajes, felicitación y catalejo,
tutorial del buzón con teclado, `zelda_fly`, entrenamiento con Orca y espada
(0x38), bosque (`look_tetra`, `AMORI_DEMO_2/1`, `MEET_TETORA`), secuestro de
Abril (`STOLENSISTER`), conversación de la playa (`yuukaigo`), escudo (0x3b,
`get_shield`), despedida (`departure_DEMO`) y llegada a A_umikz con control.
Línea final: `FULL ISLAND RUN REACHED SHIP: departure watched in this run,
EventFlag 0x2401=1`. Se conservan localmente 19 volcados del estado real, uno
por hito. El usuario no habló con Mesa ni con Abe en esta partida.

Defectos observados por el usuario durante la partida:

- **Estela de la espada gigante: corregida (0065).** Cada golpe cubría casi
  toda la pantalla de blanco. Las tablas `*_pos` de la estela (pares de puntos
  de la hoja en big-endian) se copiaban sin convertir. Reproducido con el
  estado real de la espada (`probe-H5tbhw`: el brillo medio salta de 157 a
  240 en cada golpe) y corregido (`probe-3iAs1g`: arco fino y brillo estable).
- **Ojos de Tetra en las cinemáticas** (cerrados en el rescate, abiertos en la
  caída): mismo origen que 0064. Esa partida usaba un ejecutable anterior a
  0064; falta confirmarlo visualmente en el rescate.
- **Parpadeos en la despedida** con la ventana visible: pendiente de describir
  y reproducir. Las capturas sin ventana de la despedida no los muestran.
- Música de fondo, fanfarria de objeto y voces de Link: pendientes, ya
  documentado.

## Cierre al entrar en Asoko, interior del barco (0066)

Después del hito, al cruzar la puerta del barco hacia **Asoko**, el juego se
detuvo por una verificación interna (`c_m3d_g_sph.cpp:23`, coordenadas de una
esfera fuera de rango). Origen: la cuerda `himo3`. Cuando cuelga sola usa un
valor (`f28`) que solo se calcula al agarrarla; en GameCube quedaba el resto de
un registro y en PC basura de la pila (NaN). 0066 lo inicializa a 0 en PC.

- `probe-OON7Al`: reproduce el cierre restaurando el estado real del barco.
- `probe-oxy9bx` (resultado 0): con 0066 se entra en Asoko y Link conserva el
  control; la captura muestra el interior.

El observador de la partida daba resultado 0 aunque el juego se cerró después
del hito: `tools/crash.gdb` aceptaba el éxito anterior sin mirar la señal. Ahora
informa del cierre (excepto la parada SIGTRAP con la que una prueba termina).

## Niko y la prueba de las cuerdas en Asoko (0067)

Al entrar en Asoko (bodega del barco pirata) no ocurría nada: el actor de
Zuko, Niko y Mako (`d_a_npc_p2`) no estaba descompilado, ni siquiera en la
versión oficial más reciente, y los nueve eventos de la prueba dependen de Niko.
0067 lo reconstruye entero a partir del original: parámetros, modelos, cabeza
y torso, parpadeo, mensajes, fases de la prueba y sus 15 acciones de
cinemática (interruptor, saltos a plataformas, péndulo en la cuerda, meta).

**Prueba manual del usuario (`probe-QWsZpe`), sin cierres:** desde el estado
real del barco, `P2B_INTRO` (Niko pisa el interruptor y se mueve la plataforma),
`P2B_TO_GOAL` (salta a la plataforma, se balancea en la cuerda hablando, vuelve
y salta a la meta), conversación de espera en la meta y `P2B_GOAL` al llegar.

Un controlador automático no consiguió bajar al piso inferior (se atasca en el
borde de la abertura central); el usuario hizo ese tramo y la prueba.

## Tras Asoko: llegada a la Fortaleza Maldita y cartel en blanco (0068)

Con 0067 el usuario sigue jugando en la misma partida: abre el cofre de la
meta, el barco llega a la Fortaleza Maldita (`MajyuE`), habla con los piratas,
se reproduce el lanzamiento (`majyuu_shinnyuu`) y Link aparece dentro de la
fortaleza sin espada, como en el original. Sin cierres.

**Cartel con el nombre del lugar en blanco (0068).** La imagen del cartel
(`/res/placename/pn_XX.bti`) se lee directamente del disco. Esa ruta pasaba un
nombre vacío a la conversión de formato, que reconoce los `.bti` (sin firma)
solo por la extensión. La cabecera quedaba en big-endian (`resource format not
converted`, 14.432 bytes) y el cartel salía como un rectángulo blanco. Ahora se
pasa la ruta real cuando termina en `.bti`; las demás lecturas directas no
cambian. Compilación y reproducción limpia superadas. **Falta confirmarlo en
pantalla** con el siguiente cartel: la partida en curso usa el ejecutable
anterior.

## Bloqueo del talismán en la Fortaleza Maldita (0069)

Dentro de la fortaleza, la primera llamada de Tetra por el talismán (evento
`FIRST_HINT`) se quedaba parada con las bandas de cinemática. Leyendo el
evento del disco (`tools/disc_events.py`), su cámara usa el modo `ROLLING`,
cuya función estaba vacía: no marcaba la cámara como lista ni terminaba, y Link,
`TAGHINT` y las cuentas atrás esperaban indefinidamente.

0069 reconstruye `rollingEvCamera` a partir del original: centro y ojo fijados
una vez respecto a `RelActor` según `RelUseMask`, cámara lista, y en cada
fotograma el centro se suaviza (`CtrCus`) y el ojo gira a su alrededor (`Roll`
grados por fotograma, `RadiusAdd`, `Latitude` con `TransType` 2, `Fovy`,
`Bank`). Sin `Timer` el corte termina al momento. Compilación y reproducción
limpia superadas. **Falta comprobarlo en la partida del usuario**, desde su
guardado en la fortaleza (`run-pc/card-fortress`).

`tools/play_observer.gdb` registra escenarios, eventos y equipo de una partida
jugada y, si un evento sigue activo 20 segundos, vuelca qué participante no
termina. El usuario notó la partida muy lenta con él en la fortaleza; la causa
probable es el punto de parada por fotograma, pendiente de aligerar.

## Escena del talismán superada; revisión pendiente de su cámara (0070)

Con 0069 la escena avanzaba, pero al coger el talismán la pantalla quedaba en
negro: el plano siguiente es `FIXEDPOS` con `RelUseMask` `oo`, y la versión de
PC solo aceptaba máscaras de una letra. El original solo mira la primera
letra. **Con 0070 el usuario supera la escena** (`probe-jl1KpI`).

**Pendiente de revisar (indicación del usuario):** durante la conversación,
la cámara de `ROLLING` daba vueltas completas en vertical y llegaba a mostrar a
Link desde abajo, cosa que el original no hace. Causa encontrada: `cSGlobe`
guarda la latitud en +4 y el rumbo en +6, y sus setters `U()`/`V()` escriben el
campo contrario al de sus getters; el giro se sumaba al rumbo pero se guardaba
en la latitud. 0070 reconstruye el ángulo con `Val(R, latitud, rumbo)`, de modo
que el giro es horizontal como en el original. **Falta que el usuario lo
confirme repitiendo la escena**; hasta entonces la cámara de esta toma se
considera sin validar.

## Fortaleza: cámara del talismán confirmada y conversación opcional (0071)

- **El usuario confirma la cámara `ROLLING` corregida en 0070**: gira solo en
  horizontal, como en el original.
- El fondo negro durante la conversación por el talismán es intencionado:
  `daTag_Hint_c::darkProc` oscurece cielo, niebla y escenario y una luz azul
  sale del talismán.
- La conversación opcional al volver a usar el talismán se bloqueaba: su cámara
  es un `UNITRANS` esférico con máscara `--on` y la versión de PC no aceptaba
  la `n`. 0071 la implementa como el original (refleja la toma al lado del actor
  donde ya está la cámara y la devuelve si una pared separa centro y ojo).
  **Superado por el usuario** (`probe-UPLB6a`).
