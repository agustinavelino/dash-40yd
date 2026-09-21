ALGORITMO COMUNICACION  NODO A Y PRUEBA 0 
1. Se declaran las variables, t1, t2, t4, offset, contador_resync, timestamp_a, timestamp_b con valores vacios 
declarar contador_evento=500 ms
2. Función de contador 
Contador_resync +=1 
Si contador = 30 s 
llamar funcion de resync. 
3. Se crea funcion de resync 
t1=leertimestamp en ese momento
Enviar mensaje B 
B recibe mensaje y escribe timestamp de t2 en ese momento y envia respuesta a A incluyendo t2 
A recibe y crea t4
Se calcula offset con formula usanod t2 t4 y t1
Se escibre este valor en variable offset 
4. Funcion de eventos 
Se crea variable de resultado
Se envia indicador para cortar timestamps
Se erscribe timestamp de A y timestamp de B en variables
B envia a A su timestamp, no importa cuanot tarde 
Se hace el calculo que creo que es una resta entre los timestamps  y se storea en resultado
Este resultado se guarda o se envia a lagun laod para guardar 
Esta funcion se ejecuta cada 500 ms 