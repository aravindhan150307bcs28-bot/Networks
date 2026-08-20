#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>

#define MAX 100

// --- Global Variables (Shared for convenience in this combined scope) ---
char data[MAX];
char hamming[MAX];
char received[MAX];
char original[MAX];

int dataBits = 0;
int parityBits = 0;
int totalBits = 0;

// --- Function Prototypes ---

// Shared Utilities
int isPowerOfTwo(int n);
void flushInput();

// Sender Functions
void runSender();
void getDataSender();
void calculateParityBitsCount();
void placeDataBitsSender();
void calculateParityBitsValueSender();
void displaySender();
void writeToChannel();

// Receiver Functions
void runReceiver();
void readFromChannel();
void promptForReceivedCode();
void detectAndCorrect();
void extractOriginalData();
void displayReceiver();

// --- Main Function ---
int main()
{
    int choice;

    do
    {
        printf("\n========================================");
        printf("\n        HAMMING CODE SIMULATOR");
        printf("\n========================================");
        printf("\n1. Run Full Process (Sender -> Channel -> Receiver)");
        printf("\n2. Exit");
        printf("\nEnter your choice : ");

        if (scanf("%d", &choice) != 1)
        {
            choice = 2; // Treat bad input as exit
        }
        flushInput(); // Clear the newline left by scanf

        switch (choice)
        {
            case 1:
                runSender();
                printf("\n----------------------------------------\n");
                runReceiver();
                break;
            case 2:
                printf("\nExiting...\n");
                break;
            default:
                printf("\nInvalid choice. Please try again.\n");
        }
    }
    while (choice != 2);

    return 0;
}

// --- Shared Utility Functions ---

// Clears the input buffer to prevent issues with subsequent scanf calls
void flushInput()
{
    int c;
    while ((c = getchar()) != '\n' && c != EOF);
}

// Checks if an integer n is a power of two (1, 2, 4, 8, ...)
int isPowerOfTwo(int n)
{
    if (n <= 0) return 0;
    return (n & (n - 1)) == 0;
}

// --- Sender Implementation ---

void runSender()
{
    printf("\n========================================");
    printf("\n        HAMMING CODE - SENDER");
    printf("\n========================================\n");

    getDataSender();
    calculateParityBitsCount();
    placeDataBitsSender();
    calculateParityBitsValueSender();
    displaySender();
    writeToChannel();
}

void getDataSender()
{
    printf("\nEnter Binary Data (e.g., 1011) : ");
    scanf("%s", data);
    flushInput();

    dataBits = strlen(data);
    printf("\nNumber of Data Bits : %d\n", dataBits);
}

// Calculates the minimum number of parity bits (r) needed for d data bits
void calculateParityBitsCount()
{
    parityBits = 0;
    // Formula: 2^r >= d + r + 1
    while (pow(2, parityBits) < (dataBits + parityBits + 1))
    {
        parityBits++;
    }

    totalBits = dataBits + parityBits;

    printf("\nParity Bits Required : %d", parityBits);
    printf("\nTotal Hamming Bits   : %d\n", totalBits);
}

// Places data bits at non-power-of-two positions; initializes parity positions to '0'
void placeDataBitsSender()
{
    int i, j = 0;
    printf("\nPlacing Data and Parity Bits (P = Parity, D = Data):\n");
    for (i = 1; i <= totalBits; i++)
    {
        if (isPowerOfTwo(i))
        {
            hamming[i] = '0'; // Temporary placeholder
            printf("Position %2d -> P\n", i);
        }
        else
        {
            hamming[i] = data[j++];
            printf("Position %2d -> D (%c)\n", i, hamming[i]);
        }
    }
}

// Calculates the value (0 or 1) for each parity bit using Even Parity
void calculateParityBitsValueSender()
{
    int i, j, k;
    int parity;

    printf("\nCalculating Even Parity Bits...\n");
    for (i = 0; i < parityBits; i++)
    {
        int position = (int)pow(2, i);
        parity = 0;

        /*
         * Check bits covered by this parity bit.
         * The inner loops select bits: starts at 'position',
         * takes 'position' bits, skips 'position' bits, and repeats.
         */
        for (j = position; j <= totalBits; j += 2 * position)
        {
            for (k = j; k < j + position && k <= totalBits; k++)
            {
                // We skip the parity bit itself when calculating its value
                if (k != position)
                {
                    if (hamming[k] == '1')
                        parity++;
                }
            }
        }

        if (parity % 2 == 0)
            hamming[position] = '0';
        else
            hamming[position] = '1';

        printf("P%d (Pos %2d) = %c\n", i + 1, position, hamming[position]);
    }
}

void displaySender()
{
    int i; // <--- FIXED: Declaration added here
    printf("\n====================================");
    printf("\n         HAMMING SENDER");
    printf("\n====================================");
    printf("\nOriginal Data : %s", data);
    printf("\nGenerated Hamming Code : ");
    for (i = 1; i <= totalBits; i++)
        printf("%c", hamming[i]);
    printf("\n");
}

// Writes the generated code and metadata to a file to simulate a noisy channel
void writeToChannel()
{
    FILE *fp;
    int i;
    fp = fopen("channel.txt", "w");
    if (fp == NULL)
    {
        printf("\nError opening channel.txt for writing\n");
        return;
    }
    // Write metadata
    fprintf(fp, "%d %d %d\n", dataBits, parityBits, totalBits);
    // Write the code string
    for (i = 1; i <= totalBits; i++)
        fprintf(fp, "%c", hamming[i]);
    fprintf(fp, "\n");
    fclose(fp);
    printf("\nHamming Code written to channel.txt successfully.\n");
}

// --- Receiver Implementation ---

void runReceiver()
{
    printf("\n========================================");
    printf("\n        HAMMING CODE - RECEIVER");
    printf("\n========================================\n");

    readFromChannel();
    promptForReceivedCode();
    detectAndCorrect();
    extractOriginalData();
    displayReceiver();
}

// Reads the code and metadata from the channel file
void readFromChannel()
{
    FILE *fp;
    int i;
    fp = fopen("channel.txt", "r");
    if (fp == NULL) {
        printf("\nError: channel.txt not found. Run sender first.\n");
        exit(1); // Exit if sender hasn't run
    }
    // Read metadata
    if (fscanf(fp, "%d %d %d", &dataBits, &parityBits, &totalBits) != 3)
    {
         printf("\nError reading metadata from channel.txt\n");
         fclose(fp);
         exit(1);
    }
    // Read the Hamming code string
    char tempCode[MAX];
    if (fscanf(fp, "%s", tempCode) != 1)
    {
        printf("\nError reading Hamming code from channel.txt\n");
        fclose(fp);
        exit(1);
    }
    // Store it into the global 'received' array (1-indexed)
    for(i=0; i < totalBits; i++)
    {
        received[i+1] = tempCode[i];
    }
    received[totalBits + 1] = '\0';

    fclose(fp);

    printf("\nChannel Data Loaded:");
    printf("\nData Bits   : %d", dataBits);
    printf("\nParity Bits : %d", parityBits);
    printf("\nTotal Bits  : %d\n", totalBits);

    printf("\nHamming Code read from channel : ");
    for (i = 1; i <= totalBits; i++)
        printf("%c", received[i]);
    printf("\n");
}

// Allows the user to manually alter the received code to simulate transmission errors
void promptForReceivedCode()
{
    char choice;
    int i; // <--- FIXED: Declaration added here
    printf("\nDo you want to introduce an error manually? (y/n) : ");
    scanf(" %c", &choice);
    flushInput();

    if (choice == 'y' || choice == 'Y')
    {
        int pos;
        printf("Enter bit position (1-%d) to flip (0->1 or 1->0) : ", totalBits);
        if(scanf("%d", &pos) == 1 && pos >= 1 && pos <= totalBits)
        {
            if (received[pos] == '0')
                received[pos] = '1';
            else
                received[pos] = '0';
            printf("Bit at position %d flipped.\n", pos);
        }
        else
        {
            printf("Invalid position. No error introduced.\n");
            flushInput(); // Important: clean up if scanf fails on int
        }
    }

    printf("\nFinal Received Code to Process : ");
    for (i = 1; i <= totalBits; i++)
        printf("%c", received[i]);
    printf("\n");
}

// Calculates syndrome to find and correct a single-bit error
void detectAndCorrect()
{
    int i, j, k;
    int parity;
    int errorPosition = 0;

    printf("\nChecking Parity Bits (Syndrome Calculation)... \n");
    for (i = 0; i < parityBits; i++)
    {
        int position = (int)pow(2, i);
        parity = 0;

        // Recalculate parity for the received code
        for (j = position; j <= totalBits; j = j + 2 * position)
        {
            for (k = j; k < j + position && k <= totalBits; k++)
            {
                if (received[k] == '1')
                    parity++;
            }
        }

        // If parity is odd, the syndrome bit is 1
        if (parity % 2 != 0)
        {
            errorPosition += position; // Add power of 2 to error position
            printf("Syndrome S%d (from P%d) : 1 (Error)\n", i+1, position);
        } else
        {
            printf("Syndrome S%d (from P%d) : 0 (OK)\n", i+1, position);
        }
    }

    if (errorPosition == 0)
    {
        printf("\nNo Error Detected.\n");
    }
    else if (errorPosition > totalBits)
    {
        // This case shouldn't happen with a single-bit error in standard Hamming(7,4)
        printf("\nMultiple Errors Detected (Syndrome = %d). Cannot correct.\n", errorPosition);
    }
    else
    {
        printf("\nError Detected at Bit Position : %d\n", errorPosition);
        // Flip the bit to correct it
        if (received[errorPosition] == '0')
            received[errorPosition] = '1';
        else
            received[errorPosition] = '0';
        printf("Error Corrected Successfully.\n");
    }
}

// Extracts only the data bits from the (potentially corrected) Hamming code
void extractOriginalData()
{
    int i;
    int j = 0;
    for (i = 1; i <= totalBits; i++)
    {
        if (!isPowerOfTwo(i))
        {
            original[j] = received[i];
            j++;
        }
    }
    original[j] = '\0'; // Null-terminate the string
}

void displayReceiver()
{
    int i; // <--- FIXED: Declaration added here
    printf("\n====================================");
    printf("\n          FINAL OUTPUT");
    printf("\n====================================");
    printf("\nCorrected Hamming Code : ");
    for (i = 1; i <= totalBits; i++)
        printf("%c", received[i]);
    printf("\n");
    printf("Original Data Extracted : %s\n", original);
}
