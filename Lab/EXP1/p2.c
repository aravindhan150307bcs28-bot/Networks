#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define TABLE_SIZE 10
#define MAX_MSG    500
#define MAX_BITS   (MAX_MSG * 8 + 1)
#define MAX_PACKETS ((MAX_BITS / 16) + 2)
#define SYN_BYTE   0x16


char urlTable[TABLE_SIZE][50];
char ipTable[TABLE_SIZE][20];
char macTable[TABLE_SIZE][20];
int  used[TABLE_SIZE] = {0};

int hashFunction(char url[])
{
    int sum = 0;
    int i;
    for (i = 0; url[i] != '\0'; i++)
        {
        sum = sum + url[i];
    }
    return sum % TABLE_SIZE;
}

void insertURL(char url[], char ip[], char mac[])
{
    int index = hashFunction(url);
    int count = 0;

    while (used[index] == 1 && count < TABLE_SIZE)
        {
        if (strcmp(urlTable[index], url) == 0)
                {
            return;
        }
        index = (index + 1) % TABLE_SIZE;
        count++;
    }

    strcpy(urlTable[index], url);
    strcpy(ipTable[index], ip);
    strcpy(macTable[index], mac);
    used[index] = 1;
}

int searchURL(char url[])
{
    int index = hashFunction(url);
    int count = 0;

    while (count < TABLE_SIZE)
        {
        if (used[index] == 1 && strcmp(urlTable[index], url) == 0)
                {
            return index;
        }
        index = (index + 1) % TABLE_SIZE;
        count++;
    }
    return -1;
}

void printTable()
{
    int i;
    printf("---------------------------------------------------\n");
    printf(" HASH TABLE (URL -> IP -> MAC)\n");
    printf("---------------------------------------------------\n");
    for (i = 0; i < TABLE_SIZE; i++)
        {
        if (used[i] == 1)
                {
            printf(" Slot %d : %-15s | %-15s | %s\n",i, urlTable[i], ipTable[i], macTable[i]);
        }
    }
    printf("---------------------------------------------------\n\n");
}

void byteToBinary(int number, char result[])
{
    int i;
    for (i = 7; i >= 0; i--)
        {
        if ((number >> i) & 1)
                {
            result[7 - i] = '1';
        }
                else
                {
            result[7 - i] = '0';
        }
    }
    result[8] = '\0';
}

void numberToBinary16(int number, char result[])
{
    int i;
    for (i = 15; i >= 0; i--)
        {
        if ((number >> i) & 1)
                {
            result[15 - i] = '1';
        }
                else
                {
            result[15 - i] = '0';
        }
    }
    result[16] = '\0';
}

/* NEW: generic N-bit binary converter (needed for DDCMP COUNT field = 14 bits) */
void numberToBinaryN(int number, int numBits, char result[])
{
    int i;
    for (i = numBits - 1; i >= 0; i--)
        {
        if ((number >> i) & 1)
                {
            result[numBits - 1 - i] = '1';
        }
                else
                {
            result[numBits - 1 - i] = '0';
        }
    }
    result[numBits] = '\0';
}

void ipToBinary32(char ip[], char result[])
{
    int a, b, c, d;
    char piece[9];
    sscanf(ip, "%d.%d.%d.%d", &a, &b, &c, &d);

    result[0] = '\0';
    byteToBinary(a, piece); strcat(result, piece);
    byteToBinary(b, piece); strcat(result, piece);
    byteToBinary(c, piece); strcat(result, piece);
    byteToBinary(d, piece); strcat(result, piece);
}

void macToBinary48(char mac[], char result[])
{
    int b0, b1, b2, b3, b4, b5;
    char piece[9];
    sscanf(mac, "%x:%x:%x:%x:%x:%x", &b0, &b1, &b2, &b3, &b4, &b5);

    result[0] = '\0';
    byteToBinary(b0, piece); strcat(result, piece);
    byteToBinary(b1, piece); strcat(result, piece);
    byteToBinary(b2, piece); strcat(result, piece);
    byteToBinary(b3, piece); strcat(result, piece);
    byteToBinary(b4, piece); strcat(result, piece);
    byteToBinary(b5, piece); strcat(result, piece);
}

void makeRandomIP(char ip[])
{
    int a = 1 + rand() % 223;
    int b = rand() % 256;
    int c = rand() % 256;
    int d = 1 + rand() % 254;
    sprintf(ip, "%d.%d.%d.%d", a, b, c, d);
}

void makeRandomMAC(char mac[])
{
    sprintf(mac, "%02X:%02X:%02X:%02X:%02X:%02X",
            rand() % 256, rand() % 256, rand() % 256,
            rand() % 256, rand() % 256, rand() % 256);
}


int main() {
    srand(time(NULL));

    printf("=====================================================\n");
    printf("     SIMPLE 4-LAYER NETWORK SIMULATOR (in binary)\n");
    printf("=====================================================\n\n");

    insertURL("google.com",    "142.250.193.14",  "3C:5A:B4:1D:9F:02");
    insertURL("youtube.com",   "142.250.72.14",   "A4:5E:60:D3:2B:19");
    insertURL("facebook.com",  "157.240.22.35",   "F0:2F:74:6B:88:11");
    insertURL("amazon.com",    "205.251.242.103", "B8:27:EB:9A:3C:44");
    insertURL("wikipedia.org", "208.80.154.224",  "00:1A:2B:3C:4D:5E");

    printTable();

    char srcUrl[50], dstUrl[50];
    printf("Enter SOURCE URL (example: google.com): ");
    scanf("%49s", srcUrl);
    printf("Enter DESTINATION URL (example: youtube.com): ");
    scanf("%49s", dstUrl);

    int srcIndex = searchURL(srcUrl);
    if (srcIndex == -1)
        {
        char newIp[20], newMac[20];
        makeRandomIP(newIp);
        makeRandomMAC(newMac);
        insertURL(srcUrl, newIp, newMac);
        srcIndex = searchURL(srcUrl);
        printf("\n(\"%s\" was not in the table, so a new IP/MAC was created)\n", srcUrl);
    }

    int dstIndex = searchURL(dstUrl);
    if (dstIndex == -1)
        {
        char newIp[20], newMac[20];
        makeRandomIP(newIp);
        makeRandomMAC(newMac);
        insertURL(dstUrl, newIp, newMac);
        dstIndex = searchURL(dstUrl);
        printf("\n(\"%s\" was not in the table, so a new IP/MAC was created)\n", dstUrl);
    }

    char srcIP[20], dstIP[20], srcMAC[20], dstMAC[20];
    strcpy(srcIP,  ipTable[srcIndex]);
    strcpy(dstIP,  ipTable[dstIndex]);
    strcpy(srcMAC, macTable[srcIndex]);
    strcpy(dstMAC, macTable[dstIndex]);

    char srcIPBin[40], dstIPBin[40], srcMACBin[60], dstMACBin[60];
    ipToBinary32(srcIP, srcIPBin);
    ipToBinary32(dstIP, dstIPBin);
    macToBinary48(srcMAC, srcMACBin);
    macToBinary48(dstMAC, dstMACBin);

    printf("\n----------------- ADDRESS RESOLUTION -----------------\n");
    printf("SOURCE      : %s -> IP %s -> MAC %s\n", srcUrl, srcIP, srcMAC);
    printf("   IP  binary  (32 bits): %s\n", srcIPBin);
    printf("   MAC binary  (48 bits): %s\n", srcMACBin);
    printf("DESTINATION : %s -> IP %s -> MAC %s\n", dstUrl, dstIP, dstMAC);
    printf("   IP  binary  (32 bits): %s\n", dstIPBin);
    printf("   MAC binary  (48 bits): %s\n", dstMACBin);
    printf("-------------------------------------------------------\n\n");

    /* LAYER 1: APPLICATION LAYER */
    FILE *file = fopen("message.txt", "r");
    if (file == NULL)
        {
        printf("ERROR: message.txt not found!\n");
        return 1;
    }

    char message[MAX_MSG];
    int msgLength = 0;
    int c;
    while ((c = fgetc(file)) != EOF && msgLength < MAX_MSG - 1)
        {
        if (c == '\n' || c == '\r')
                        continue;
        message[msgLength] = (char) c;
        msgLength++;
    }
    message[msgLength] = '\0';
    fclose(file);

    if (msgLength == 0)
        {
        printf("ERROR: message.txt is empty. Please put some text in it.\n");
        return 1;
    }

    printf("=============== APPLICATION LAYER ===============\n");
    printf("Message from file: \"%s\"\n\n", message);

    char bits[MAX_BITS];
    bits[0] = '\0';
    int i;
    for (i = 0; i < msgLength; i++)
        {
        char oneByte[9];
        byteToBinary((int)(unsigned char)message[i], oneByte);
        strcat(bits, oneByte);
        printf("  '%c'  ->  %s\n", message[i], oneByte);
    }
    int totalBits = msgLength * 8;
    printf("\nFull bitstream (%d bits): %s\n", totalBits, bits);
    printf("===================================================\n\n");

    /* LAYER 2: TRANSPORT LAYER */
    int srcPort = 1024 + rand() % (65535 - 1024 + 1);
    int dstPort = 1024 + rand() % (65535 - 1024 + 1);
    char srcPortBin[17], dstPortBin[17];
    numberToBinary16(srcPort, srcPortBin);
    numberToBinary16(dstPort, dstPortBin);

    printf("=============== TRANSPORT LAYER ===============\n");
    /*printf("Source Port      : %d  (binary: %s)\n", srcPort, srcPortBin);
    printf("Destination Port : %d  (binary: %s)\n", dstPort, dstPortBin);*/
    printf("%s%s%s",srcPortBin,dstPortBin,bits);
    printf("=================================================\n\n");

    /* LAYER 3: NETWORK LAYER */
    int packetSize = 8;
    int numPackets = totalBits / packetSize;
    if (totalBits % packetSize != 0)
        {
        numPackets++;
    }

    char packets[MAX_PACKETS][17];

    printf("=============== NETWORK LAYER ===============\n");
    printf("Total packets: %d\n\n", numPackets);

    int p;
    for (p = 0; p < numPackets; p++)
        {
        int start = p * packetSize;
        int b;
        for (b = 0; b < packetSize; b++)
                {
            int pos = start + b;
            if (pos < totalBits)
                        {
                packets[p][b] = bits[pos];
            }
                        else
                        {
                packets[p][b] = '0';
            }
        }
        packets[p][packetSize] = '\0';

        printf("Packet %d:\n", p + 1);
        /*printf("  Data      : %s\n", packets[p]);
        printf("  Src IP    : %s\n", srcIPBin);
        printf("  Dst IP    : %s\n\n", dstIPBin);*/
        printf("%s%s%s%s%s\n",srcIPBin,dstIPBin,srcPortBin,dstPortBin,packets[p]);
    }
    printf("===============================================\n\n");

    /* ======================================================================
       LAYER 4: DATA LINK LAYER  --  DDCMP BYTE COUNT APPROACH
       Frame format: SYN1 | SYN2 | CLASS | COUNT | HEADER | BODY | CRC
         SYN1,SYN2 (8+8) : sync pattern, checked at receiver
         CLASS     (8)   : type of data
         COUNT     (14)  : bit-count of the frame's data field
         HEADER    (Src IP 32 + Src MAC 48) : source address & mac address
         BODY      : Src IP + Dst IP + Src Port + Dst Port + Data
         CRC       (16)  : dummy, all zeros
       One DDCMP frame is built per 16-bit network-layer packet.
       ====================================================================== */
    char synBin[9];
    numberToBinaryN(SYN_BYTE, 8, synBin);

    char classBin[9];
    numberToBinaryN(1, 8, classBin);

    int totalFrames = 2*numPackets;
    int totalByteCount = 0;

    printf("=============== DATA LINK LAYER (DDCMP) ===============\n");
    printf("Total frames: %d\n\n", totalFrames);

    for (p = 0; p < numPackets; p++)
        {

        char *frameData = packets[p];
        int dataBits = packetSize;

        char countBin[15];
        int byteCount = dataBits / 8;
        numberToBinaryN(byteCount, 14, countBin);
        totalByteCount += byteCount;

        char headerBin[320];
        strcpy(headerBin, srcMACBin);
        strcat(headerBin, dstMACBin);
        strcat(headerBin, srcIPBin);
        strcat(headerBin, dstIPBin);
        strcat(headerBin, srcPortBin);
        strcat(headerBin, dstPortBin);

        char bodyBin[120];
        strcpy(bodyBin, frameData);

        char crcBin[17] = "0000000000000000";

        /* ---------- TRANSMITTER SIDE ---------- */
        printf("--- Frame (from Packet %d) : TRANSMITTED ---\n", p + 1);
        printf("  SYN1    : %s\n", synBin);
        printf("  SYN2    : %s\n", synBin);
        printf("  CLASS   : %s\n", classBin);
        printf("  COUNT   : %s  (byte count = %d)\n", countBin,byteCount);
        printf("  HEADER  : %s\n", headerBin);
        /*printf("    -> Src MAC  (32): %s\n", srcMACBin);
        printf("    -> Dst MAC (48): %s\n", dstMACBin);*/
        printf("  BODY   : %s\n",  bodyBin);
        /*printf("    -> Src IP   (32): %s\n", srcIPBin);
        printf("    -> Dst IP   (32): %s\n", dstIPBin);
        printf("    -> Src Port (16): %s\n", srcPortBin);
        printf("    -> Dst Port (16): %s\n", dstPortBin);
        printf("    -> Data     (%2d): %s\n", dataBits, frameData);*/
        printf("  CRC     : %s\n", crcBin);

        /* ---------- RECEIVER SIDE ---------- */
        printf("--- Frame : RECEIVER CHECK ---\n");
        if (strcmp(synBin, synBin) == 0)
        {
            printf("  SYN1 & SYN2 verified. Frame synchronization OK.\n");

            int countValue = 0;
            int bi;
            for (bi = 0; bi < 14; bi++)
            {
                countValue = countValue * 2 + (countBin[bi] - '0');
            }

            if ((countValue*8) == dataBits)
            {
                printf("  COUNT verified (%d bytes). Data accepted: %s\n\n", countValue, frameData);
            }
            else
            {
                printf("  Framing ERROR: COUNT mismatch (corrupted count).\n\n");
            }
        }
        else
        {
            printf("  SYN mismatch! Frame dropped.\n\n");
        }
    }
    printf("=========================================================\n\n");

    /* FINAL SUMMARY */
    printf("=============== SUMMARY ===============\n");
    printf("Message         : \"%s\"\n", message);
    printf("Total bits      : %d\n", totalBits);
    printf("Total packets   : %d\n", numPackets);
    printf("Total DDCMP frames : %d\n", totalFrames);
    printf("Total data byte count (sum of COUNT fields) : %d\n", totalByteCount);
    printf("=========================================\n");

    return 0;
}
