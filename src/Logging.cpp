#include "Logging.h"

/**
 * Prints a hex dump of the given data
 * @param data Pointer to the data to dump
 * @param length Length of the data in bytes
 */
void printHexDump(const uint8_t *data, size_t length)
{
    if (data == nullptr || length == 0) {
        log_i("HexDump: Empty data");
        return;
    }
    
    // Cabecera del dump hexadecimal
    log_i("Hexdump of %u bytes:", length);
    log_i("Offset   | 00 01 02 03 04 05 06 07  08 09 0A 0B 0C 0D 0E 0F | ASCII");
    log_i("---------|--------------------------------------------------|----------------");
    
    char line[128]; // Buffer para la línea completa: offset + hex + ascii
    char hex[64];   // Parte hexadecimal de la línea
    char ascii[17]; // Parte ASCII de la línea
    
    for (size_t i = 0; i < length; i += 16) {
        // Inicializar buffers
        memset(line, 0, sizeof(line));
        memset(hex, 0, sizeof(hex));
        memset(ascii, ' ', sizeof(ascii) - 1);
        ascii[16] = '\0';
        
        // Formatear offset
        sprintf(line, "%08lX | ", (unsigned long)i);
        
        // Procesar hasta 16 bytes en esta línea
        for (size_t j = 0; j < 16; j++) {
            if (i + j < length) {
                // Añadir byte en hex
                char byteHex[4];
                sprintf(byteHex, "%02X ", data[i + j]);
                strcat(hex, byteHex);
                
                // Añadir representación ASCII
                ascii[j] = (data[i + j] >= 32 && data[i + j] <= 126) ? data[i + j] : '.';
            } else {
                // Padding para bytes faltantes
                strcat(hex, "   ");
                ascii[j] = ' ';
            }
            
            // Espacio extra entre grupos de 8 bytes
            if (j == 7) {
                strcat(hex, " ");
            }
        }
        
        // Combinar todo en una línea
        strcat(line, hex);
        strcat(line, "| ");
        strcat(line, ascii);
        
        // Imprimir la línea completa
        log_i("%s", line);
    }
    
    log_i("---------|--------------------------------------------------|----------------");
}

