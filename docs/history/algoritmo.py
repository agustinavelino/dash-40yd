#Algoritmo comunicación nodo A (Prueba 0)
import time 

#Declarar las variables
t1 , t2 , t3 , t4 = None, None, None, None
contador_resync = 0
timestamp_a, timestamp_b = None, None
contador_evento = 500
offset = 0

#Funcion de contador
def contador():
    contador_resync+=1
    if contador_resync==30:
        resync()

#Funcion de resync 
def resync():
    t1=leer_timestamp()
    enviar_mensaje_B()
    recibir_mensaje_B()
    t4=timestamp_cuando_llegaB()
    offset=((t2-t1)+(t3-t4))/2

#Funcion de eventos 
def eventos():
    enviar_pulso()
    timestamp_a=ahora
    timestamp_b=enviar_timestamp
    resultado = timestamp_a -(timestamp_b-offset)



