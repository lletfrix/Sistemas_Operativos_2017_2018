#-----------------------
# IMPORTANTE: Los valores de los parametros de los ejercicio?_test deben ajustarse.
# Se asume que los ficheros fuente se llaman ejercicio1.c, ejercicio2.c,...,ordenar.h
#-----------------------
CC = gcc
CFLAGS = -g
LDLIBS = -lm -pthread
EXE = ejercicio4a ejercicio4b ejercicio5a ejercicio5b ejercicio6 ejercicio8 ejercicio9 ejercicio12a ejercicio12b ejercicio13a ejercicio13b
BUILD_DIR = build

all : $(EXE)

.PHONY : clean
clean :
	rm -f build/*.o core $(EXE)

$(EXE) :
	@echo "#---------------------------"
	@echo "# Generando $@ "
	@echo "# Depende de $^"
	@echo "# Ha cambiado $<"
	$(CC) $(CFLAGS) -o $(BUILD_DIR)/$@ $@.c $(LDLIBS)