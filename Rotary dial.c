// Kód programu
/* Podle schématu zapojení (Obrázek 4: Schéma zapojení telefonu s rotační číselnicí), kde
   je telefon připojen k pinu D0 (vstup/výstup), který je spojen s portem P0 mikrokontroleru
   C8051F120DK. Dále je připojen přes port P2 k čtyřmístnému LCD displeji, který zobrazuje
   čísla z impulzů rotačního telefonu. */
   
#include "C8051F120.h" // Vložení hlavičkového souboru pro mikrokontrolér C8051F120

extern void Init_Device(void); // Deklarace funkce pro inicializaci zařízení
void delay(int ms); // Deklarace funkce pro zpoždění

char count = 0; // Globální proměnná pro počítání pulzů od číselníku
long phoneNumber = 0; // Proměnná pro indikaci, zda je volba čísla dokončena
sbit button = P0^0; // Definice tlačítka připojeného k portu P0, bit 0

char zn[10] = {0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09}; // Pole znaků pro výstup na LCD

void readingpulses(void) interrupt 0 { // Funkce pro čtení pulzů, přerušení 0
    if (!button) { // Pokud je tlačítko stisknuto, signál je na nízké úrovni (button==0)
        count++;
    }
}

void delay(int ms) { // Funkce pro zpoždění v milisekundách
    int i, j;
    for (i = 0; i < ms; i++) {
        for (j = 0; j < 1275; j++) {
        }
    }
}

// Funkce pro odeslání výstupu na LCD displej
void lcd(int vstup) {
    P2 = zn[vstup % 10] | 0x10;       // Nastavení prvního čísla na displeji
    delay(1);
    P2 = zn[vstup % 100 / 10] | 0x20; // Nastavení druhého čísla na displeji
    delay(1);
    P2 = zn[vstup % 1000 / 100] | 0x40; // Nastavení třetího čísla na displeji
    delay(1);
    P2 = zn[vstup % 10000 / 1000] | 0x80; // Nastavení čtvrtého čísla na displeji
    delay(1);
}

// Hlavní funkce obsahující primární logiku programu
void main(void) {
    Init_Device(); // Inicializace zařízení
    IT0 = 1; // Nastavení typu přerušení
    EX0 = 1; // Povolení externího přerušení 0
    EA = 1; // Povolení globálních přerušení

    while (1) { // Neustálé kontrolování stavu číselníku a aktualizace LCD v nekonečné smyčce
        while (!button) {
            delay(100);
            while (!button); // Ujistěme se, že tlačítko je úplně uvolněné

            if (count < 11) {
                delay(100);
                phoneNumber = phoneNumber * 10; // Násobení skutečného čísla desítkou pro posunutí doleva o jednu pozici z pravého okraje na LCD displeji.
                phoneNumber = phoneNumber + count; // Přidání hodnoty pulzu jako nové cifry
                phoneNumber = phoneNumber % 10000; // Omezení telefonního čísla na čtyři cifry
                lcd(phoneNumber); // Zobrazení čísla na LCD
            }
            count = 0; // Resetování počtu na nulu
        }
    }
}
