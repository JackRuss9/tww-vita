# Progreso de Wind Waker nativo en PC

Objetivo: arrancar por el menú original, elegir o crear una partida, introducir
el nombre, reproducir la introducción y completar Isla Initia hasta embarcar.
La versión de PS Vita queda aplazada hasta conseguirlo en PC.

**El objetivo todavía no está conseguido.** Poder caminar en una prueba que
salta las escenas no equivale a tener la isla completa jugable.

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
| Menú, tarjeta nueva, nombre e historia ilustrada | El recorrido automatizado llega al evento original `awake` de la isla | Aún se bloquea antes de entregar el control |
| Salida de audio real | SDL abre a 32 kHz; la grabación contiene señal sin saturación en el tramo medido | Se han observado interrupciones; faltan funciones de música y efectos |
| Pruebas numéricas de temporizador y animación | Superadas | Casos de cálculo, no prueba de juego completo |
| Regresión del mezclador DSP | Superada: retornos, filtros, realimentación, liberación y osciladores | No comprueba fidelidad de Wind Waker frente a la consola |
| Aplicación de parches en fuentes limpias | Superada: 38 de tww y 3 de sms-port | No reemplaza los árboles locales existentes |
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
   cabecera; pasa la prueba sintética y compila. Su validación del recorrido
   completo está en curso.
6. Audio: la cola SDL se ha vaciado durante pruebas, de forma compatible con
   los cortes oídos. Se llena la cola inicial antes de reproducir y se
   añaden estadísticas de interrupciones con `SMS_AUDIO_STATS=1`. La prueba
   `probe-oDQ0o0`, sin depurador pero concurrente con otro diagnóstico, registra
   tres interrupciones iniciales: 331 ms en total, la mayor de 279 ms; el
   contador permanece estable en los siguientes intervalos observados. El
   llenado inicial no resuelve por sí solo los cortes durante las cargas.
   `JAIZelBasic::bgmStart` y `seStart` siguen sin implementar; el DSP HLE tampoco
   implementa el comando de retardo Dolby `0x8d`. El audio no se considera resuelto.

## Cómo repetir las comprobaciones

```sh
./build-pc.sh
python3 check/test_animation.py
python3 check/test_jstudio.py
g++ -std=c++11 -O2 -Iport/include check/test_timing.cpp -o /tmp/tww-timing-test
/tmp/tww-timing-test

# Recorrido original mediante pulsaciones, sin inyectar escenas ni posiciones:
./tools/probe-pc.sh --audio --seconds 420 --gdb "$PWD/tools/normal_flow.gdb"

# Diagnóstico sin ventana, con reloj determinista; no valida audio audible:
./tools/probe-pc.sh --headless --deterministic --seconds 300 \
  --gdb "$PWD/tools/normal_flow.gdb"
```

El conductor deja reproducirse la historia ilustrada completa. Un tiempo
límite (estado 124) no es un resultado satisfactorio por sí solo: hay que leer
el registro. La opción `--audio` añade un WAV y estadísticas de interrupciones;
la grabación de las muestras mezcladas no incluye necesariamente las pausas
que produce quedarse sin datos en el dispositivo de salida.

## Próximos hitos

- Superar la escena de despertar y comprobar la entrega real del control.
- Corregir cámaras, diálogos y actores que bloqueen las tareas de la isla.
- Comprobar espada, escudo, bosque y rescate, cambios de habitación, regreso,
  despedida y embarque. Ninguno se considera validado todavía.
- Medir y corregir cortes de sonido sin usar el depurador para la prueba final;
  recuperar música, efectos y voces necesarios y comprobar sus transiciones.
- Publicar cada avance probado con comandos, resultado y fallos pendientes.

No hay todavía una estimación fiable de sesiones: faltan partes del código
original, además de errores de adaptación a PC. La estimación se revisará
cuando se complete el primer recorrido real y se identifiquen sus bloqueos.
