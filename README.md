# gint uwurandom
This is a "port" of [uwurandom](https://github.com/valadaptive/uwurandom) for the fx-CG10/20/50 (and if I wrote this right the SH4 fx-9860 lineup).
It was mostly done to test the serial driver(9600baud, 8N1) I made, as such, you need to build this with the `gint-test/serial-toying` branch right now, 
but you don't actually need anything hooked up to it to just see the random output. It's under MIT btw.
