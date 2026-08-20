#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define MAX_TOKENS 100
#define TOKEN_LEN  10

int i,j;
int tokenize(char line[], char tokens[][TOKEN_LEN])
{
    int count = 0;
    char *tok = strtok(line, " \t\n");
    while (tok != NULL && count < MAX_TOKENS)
    {
        strcpy(tokens[count], tok);
        count++;
        tok = strtok(NULL, " \t\n");
    }
    return count;
}


int stuffing(char in[][TOKEN_LEN], int inCount, char out[][TOKEN_LEN])
{
    j = 0;
    for (i = 0; i < inCount; i++)
    {
        if (strcmp(in[i], "DLE") == 0 ||
            strcmp(in[i], "STX") == 0 ||
            strcmp(in[i], "ETX") == 0)
        {
            strcpy(out[j], "DLE");
            j++;
        }
        strcpy(out[j], in[i]);
        j++;
    }
    return j;
}

int destuffing(char in[][TOKEN_LEN], int inCount, char out[][TOKEN_LEN])
{
    j = 0;
    for (i = 0; i < inCount; i++)
    {
        if (strcmp(in[i], "DLE") == 0 && i + 1 < inCount &&
            (strcmp(in[i + 1], "DLE") == 0 ||
             strcmp(in[i + 1], "STX") == 0 ||
             strcmp(in[i + 1], "ETX") == 0))
        {
            strcpy(out[j], in[i + 1]);
            j++;
            i++;
        }
        else
        {
            strcpy(out[j], in[i]);
            j++;
        }
    }
    return j;
}


void joinTokens(char tokens[][TOKEN_LEN], int count, char dest[])
{
    dest[0] = '\0';
    for (i = 0; i < count; i++)
    {
        strcat(dest, tokens[i]);
        if (i != count - 1) strcat(dest, " ");
    }
}

void stringToBinary(char str[], char binaryStr[])
{
    int idx = 0;
    for (i = 0; str[i] != '\0'; i++)
    {
        for (j = 7; j >= 0; j--)
            binaryStr[idx++] = ((str[i] >> j) & 1) ? '1' : '0';
    }
    binaryStr[idx] = '\0';
}

void binaryToString(char binaryStr[], char str[])
{
    int idx = 0;
    for (i = 0; binaryStr[i] != '\0'; i += 8)
    {
        char c = 0;
        for (j = 0; j < 8; j++)
            c = (c << 1) | (binaryStr[i + j] - '0');
        str[idx++] = c;
    }
    str[idx] = '\0';
}

int main()
{
    char header[50];
    char line[500];

    char bodyTokens[MAX_TOKENS][TOKEN_LEN];
    char stuffedTokens[MAX_TOKENS][TOKEN_LEN];
    char destuffedTokens[MAX_TOKENS][TOKEN_LEN];
    char recvTokens[MAX_TOKENS][TOKEN_LEN];

    char bodyStr[500], stuffedStr[500], destuffedStr[500];
    char binaryBody[4000], receivedBinaryBody[4000], receivedStuffedStr[500];

    printf("========== BISYNC CHARACTER STUFFING ==========\n");

    printf("Enter Header : ");
    scanf("%s", header);
    getchar(); /* consume leftover newline */

    printf("Enter Body (space separated, e.g. STX DLE ETX) : ");
    fgets(line, sizeof(line), stdin);

    int bodyCount = tokenize(line, bodyTokens);
    joinTokens(bodyTokens, bodyCount, bodyStr);

    int stuffedCount = stuffing(bodyTokens, bodyCount, stuffedTokens);
    joinTokens(stuffedTokens, stuffedCount, stuffedStr);

    printf("\n------------ SENDER ------------\n");

    printf("Original Frame :\n");
    printf("SYN SYN SOH %s STX %s ETX CRC\n", header, bodyStr);

    printf("\nStuffed Frame :\n");
    printf("SYN SYN SOH %s STX %s ETX CRC\n", header, stuffedStr);

    stringToBinary(stuffedStr, binaryBody);
    printf("\nBinary Stuffed Body :\n%s\n", binaryBody);

    FILE *fp = fopen("message.txt", "w");
    if (fp != NULL)
    {
        fprintf(fp, "%s", binaryBody);
        fclose(fp);
        printf("\n[Binary message stored in message.txt]\n");
    }

    printf("\n----------- RECEIVER -----------\n");

    fp = fopen("message.txt", "r");
    if (fp != NULL)
    {
        fscanf(fp, "%s", receivedBinaryBody);
        fclose(fp);
        printf("[Binary message read from message.txt]\n");
    }
    else
    {
        strcpy(receivedBinaryBody, binaryBody);
    }

    binaryToString(receivedBinaryBody, receivedStuffedStr);

    printf("\nReceived Frame :\n");
    printf("SYN SYN SOH %s STX %s ETX CRC\n", header, receivedStuffedStr);

    int recvCount = tokenize(receivedStuffedStr, recvTokens);

    int destuffedCount = destuffing(recvTokens, recvCount, destuffedTokens);
    joinTokens(destuffedTokens, destuffedCount, destuffedStr);

    printf("\nDestuffed Frame :\n");
    printf("SYN SYN SOH %s STX %s ETX CRC\n", header, destuffedStr);

    return 0;
}
