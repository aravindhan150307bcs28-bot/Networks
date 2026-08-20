#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define TABLE_SIZE 10
#define MAX_MSG    500
#define MAX_BITS   (MAX_MSG * 8 + 1)
#define MAX_PACKETS ((MAX_BITS / 16) + 2)
#define SYN_BYTE   0x16

#define CRC_GEN     "10011"
#define CRC_GEN_LEN 5
#define CRC_BITS    4

// Global Address Tables
char urlTable[TABLE_SIZE][50];
char ipTable[TABLE_SIZE][20];
char macTable[TABLE_SIZE][20];
int  used[TABLE_SIZE] = {0};

// --- HASH TABLE FUNCTIONS ---
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
    printf("\n=============================================\n");
    printf("     ROUTING LOOKUP TABLE (URL -> IP -> MAC)  \n");
    printf("=============================================\n");
    for (i = 0; i < TABLE_SIZE; i++)
    {
        if (used[i] == 1)
        {
            printf(" Slot [%d] : %s --> IP: %s --> MAC: %s\n", i, urlTable[i], ipTable[i], macTable[i]);
        }
    }
    printf("=============================================\n\n");
}

// --- BINARY CONVERSION HELPERS ---
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

// --- NETWORK GENERATOR HELPERS ---
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

// --- CRC-4 ERROR DETECTION FUNCTIONS ---
void modulo2Divide(char data[], int dataLen, char remainder[])
{
    char work[600];
    int i, j;

    strcpy(work, data);

    for (i = 0; i <= dataLen - CRC_GEN_LEN; i++)
    {
        if (work[i] == '1')
        {
            for (j = 0; j < CRC_GEN_LEN; j++)
            {
                work[i + j] = ((work[i + j] - '0') ^ (CRC_GEN[j] - '0')) + '0';
            }
        }
    }

    strcpy(remainder, work + (dataLen - CRC_BITS));
    remainder[CRC_BITS] = '\0';
}

void crcEncode(char dataword[], int k, char crcOut[], char codewordOut[])
{
    char augmented[600];

    strcpy(augmented, dataword);
    strcat(augmented, "0000");

    modulo2Divide(augmented, k + CRC_BITS, crcOut);

    strcpy(codewordOut, dataword);
    strcat(codewordOut, crcOut);
}

int crcCheck(char codeword[], int n, char syndromeOut[])
{
    int i;
    modulo2Divide(codeword, n, syndromeOut);

    for (i = 0; i < CRC_BITS; i++)
    {
        if (syndromeOut[i] != '0')
        {
            return 0;
        }
    }
    return 1;
}

// --- MAIN EXECUTION PIPELINE ---
int main() {
    srand(time(NULL));

    printf("=================================================================\n");
    printf("          STARTING 4-LAYER COMPUTER NETWORK SIMULATOR          \n");
    printf("=================================================================\n");

    // Initialize Default Domain Routing Entries
    insertURL("google.com",    "142.250.193.14",  "3C:5A:B4:1D:9F:02");
    insertURL("youtube.com",   "142.250.72.14",   "A4:5E:60:D3:2B:19");
    insertURL("facebook.com",  "157.240.22.35",   "F0:2F:74:6B:88:11");
    insertURL("amazon.com",    "205.251.242.103", "B8:27:EB:9A:3C:44");
    insertURL("wikipedia.org", "208.80.154.224",  "00:1A:2B:3C:4D:5E");

    printTable();

    // User Endpoint Configuration
    char srcUrl[50], dstUrl[50];
    printf("Enter SOURCE web address (e.g., google.com): ");
    scanf("%49s", srcUrl);
    printf("Enter DESTINATION web address (e.g., youtube.com): ");
    scanf("%49s", dstUrl);

    int srcIndex = searchURL(srcUrl);
    if (srcIndex == -1)
    {
        char newIp[20], newMac[20];
        makeRandomIP(newIp);
        makeRandomMAC(newMac);
        insertURL(srcUrl, newIp, newMac);
        srcIndex = searchURL(srcUrl);
        printf("\n[New Device Detected] Dynamic entry added for '%s'\n", srcUrl);
    }

    int dstIndex = searchURL(dstUrl);
    if (dstIndex == -1)
    {
        char newIp[20], newMac[20];
        makeRandomIP(newIp);
        makeRandomMAC(newMac);
        insertURL(dstUrl, newIp, newMac);
        dstIndex = searchURL(dstUrl);
        printf("[New Device Detected] Dynamic entry added for '%s'\n", dstUrl);
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

    printf("\n[STEP 1: ADDRESS RESOLUTION]\n");
    printf("   * Source Machine      : %s --> IP: %s (%s) --> MAC: %s\n", srcUrl, srcIP, srcIPBin, srcMACBin);
    printf("   * Destination Machine : %s --> IP: %s (%s) --> MAC: %s\n", dstUrl, dstIP, dstIPBin, dstMACBin);

    /* LAYER 1: APPLICATION LAYER */
    FILE *file = fopen("message.txt", "r");
    if (file == NULL)
    {
        printf("\nERROR: 'message.txt' not found! Please create it.\n");
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
        printf("\nERROR: 'message.txt' is empty!\n");
        return 1;
    }

    printf("\n============ LAYER 1: APPLICATION LAYER ============\n");
    printf("Original Text Message : \"%s\"\n", message);
    printf("Translating Characters to Binary Bytes:\n");

    char bits[MAX_BITS];
    bits[0] = '\0';
    int i;
    for (i = 0; i < msgLength; i++)
    {
        char oneByte[9];
        byteToBinary((int)(unsigned char)message[i], oneByte);
        strcat(bits, oneByte);
        printf("   * '%c' --> %s\n", message[i], oneByte);
    }
    int totalBits = msgLength * 8;
    printf("\nResulting Pure Data Bitstream (%d bits):\n   %s\n", totalBits, bits);

    /* LAYER 2: TRANSPORT LAYER */
    int srcPort = 1024 + rand() % (65535 - 1024 + 1);
    int dstPort = 1024 + rand() % (65535 - 1024 + 1);
    char srcPortBin[17], dstPortBin[17];
    numberToBinary16(srcPort, srcPortBin);
    numberToBinary16(dstPort, dstPortBin);

    printf("\n============ LAYER 2: TRANSPORT LAYER ============\n");
    printf("Ports Assigned -> Source Port: %d | Destination Port: %d\n", srcPort, dstPort);
    printf("Adding Port Headers [SrcPort + DstPort + Data]:\n");
    printf("   %s [%s] [%s]\n", srcPortBin, dstPortBin, bits);

    /* LAYER 3: NETWORK LAYER */
    int packetSize = 16;
    int numPackets = totalBits / packetSize;
    if (totalBits % packetSize != 0)
    {
        numPackets++;
    }

    char packets[MAX_PACKETS][17];

    printf("\n============ LAYER 3: NETWORK LAYER ============\n");
    printf("Splitting stream into %d separate raw packets (%d bits each):\n", numPackets, packetSize);

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
        packets[p][b] = '0'; // Padding missing bits
    }
}
packets[p][packetSize] = '\0';

printf("   Packet #%02d Assembly [SrcIP + DstIP + SrcPort + DstPort + PacketData]:\n", p + 1);
printf("      %s [%s] [%s] [%s] [%s]\n", srcIPBin, dstIPBin, srcPortBin, dstPortBin, packets[p]);
}

/* LAYER 4: DATA LINK LAYER */
char synBin[9];
numberToBinaryN(SYN_BYTE, 8, synBin);

char classBin[9];
numberToBinaryN(1, 8, classBin);

int framesPerPacket = 2;
int halfSize = packetSize / framesPerPacket;
int totalFrames = numPackets * framesPerPacket;
int totalByteCount = 0;
int framesOK = 0, framesError = 0;
int frameNum = 0;

printf("\n============ LAYER 4: DATA LINK LAYER (DDCMP + CRC-4) ============\n");
printf("Configuration:\n");
printf("   * CRC Polynomial Rule : x^4 + x + 1 (Bit pattern: %s)\n", CRC_GEN);
printf("   * Framing Rule         : Split every network packet into %d data link frames (%d bits each)\n", framesPerPacket, halfSize);
printf("   * Total Frames to Send : %d\n", totalFrames);

for (p = 0; p < numPackets; p++)
{
    int h;
    for (h = 0; h < framesPerPacket; h++)
    {
        frameNum++;

        char frameData[9];
        memcpy(frameData, packets[p] + (h * halfSize), halfSize);
        frameData[halfSize] = '\0';
        int dataBits = halfSize;

        char countBin[15];
        int byteCount = dataBits / 8;
        numberToBinaryN(byteCount, 14, countBin);
        totalByteCount += byteCount;

        char headerBin[120];
        strcpy(headerBin, srcMACBin);
        strcat(headerBin, dstMACBin);

        char bodyBin[250];
        strcpy(bodyBin, srcIPBin);
        strcat(bodyBin, dstIPBin);
        strcat(bodyBin, srcPortBin);
        strcat(bodyBin, dstPortBin);
        strcat(bodyBin, frameData);
        int bodyLen = (int) strlen(bodyBin);

        char crcBin[CRC_BITS + 1];
        char codewordBin[300];
        crcEncode(bodyBin, bodyLen, crcBin, codewordBin);
        int codewordLen = bodyLen + CRC_BITS;

        printf("\n-------------------------------------------------------------\n");
        printf(" TRANSMITTING FRAME [%d/%d] (Packet %d, segment %d)\n", frameNum, totalFrames, p + 1, h + 1);
        printf("-------------------------------------------------------------\n");
        printf("   [Sync Blocks] SYN1: %s | SYN2: %s | CLASS: %s\n", synBin, synBin, classBin);
        printf("   [Size Tracker] COUNT Bits: %s (Translates to %d Bytes of data)\n", countBin, byteCount);
        printf("   [Hardware IDs] MAC Path Headers: %s\n", headerBin);
        printf("   [Payload Data] Network Body Structure:\n     %s\n", bodyBin);
        printf("   [Error Check] Generated CRC-4 Bits : %s\n", crcBin);
        printf("   FULL DATA FRAME DEPLOYED ON CABLE:\n     %s\n", codewordBin);

        char receivedCodeword[300];
        strcpy(receivedCodeword, codewordBin);

        printf("\n RECEIVER END CHECKPOINT FOR FRAME [%d]\n", frameNum);
        if (strcmp(synBin, synBin) == 0)
        {
            printf("   SUCCESS: Timing beacons (SYN1/SYN2) match up! Frame alignment confirmed.\n");

            int countValue = 0;
            int bi;
            for (bi = 0; bi < 14; bi++)
            {
                countValue = countValue * 2 + (countBin[bi] - '0');
            }

            if ((countValue * 8) == dataBits)
            {
                printf("   SUCCESS: Expected payload data size metrics verified (%d bytes).\n", countValue);

                char syndrome[CRC_BITS + 1];
                if (crcCheck(receivedCodeword, codewordLen, syndrome))
                {
                    printf("   SUCCESS: CRC-4 math evaluation syndrome calculation = %s\n", syndrome);
                    printf("      [Result]: Safe! Zero errors found. Extracted string bits: %s\n", frameData);
                    framesOK++;
                }
                else
                {
                    printf("   FAILURE: CRC Error! Syndrome = %s. Data corrupted in flight, throwing away frame.\n", syndrome);
                    framesError++;
                }
            }
            else
            {
                printf("   FAILURE: Data layout mismatch corruption detected via structural counters.\n");
                framesError++;
            }
        }
        else
        {
            printf("   FAILURE: Framing synchronization failed. Dropping unexpected signals.\n");
            framesError++;
        }
    }
}

/* SUMMARY REPORT */
printf("\n=============================================================\n");
printf("                    SIMULATION RESULTS                     \n");
printf("=============================================================\n");
printf(" Original message text   : \"%s\"\n", message);
printf(" Total bits processed     : %d bits\n", totalBits);
printf(" Network packets wrapped  : %d packets\n", numPackets);
printf(" Data link layer frames  : %d frames generated\n", totalFrames);
printf(" Frames saved successfully: %d\n", framesOK);
printf(" Broken/Discarded frames  : %d\n", framesError);
printf("=============================================================\n\n");

return 0;
}
