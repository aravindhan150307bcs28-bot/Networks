#include <stdio.h>
#include <stdlib.h>

#define ROWS 4
#define COLS 4

// Global matrices to hold data and parity bits
// Using [ROWS+1][COLS+1] to accommodate the extra parity row and column
int dataMatrix[ROWS + 1][COLS + 1];
int receivedMatrix[ROWS + 1][COLS + 1];

// --- Function Prototypes ---
void flushInput();
void displayMatrix(int matrix[ROWS + 1][COLS + 1], const char* title);

// Sender Functions
void runSenderProcess();
void getDataSender();
void calculateParitySender();
void writeToChannel();

// Receiver Functions
void runReceiverProcess();
void readFromChannel();
void introduceErrorReceiver();
void checkParityReceiver();

// --- Main Function ---
int main()
{
    int choice;

    do
    {
        printf("\n========================================");
        printf("\n       2D PARITY CODE SIMULATOR");
        printf("\n========================================");
        printf("\n1. Run Full Process (Sender -> Channel -> Receiver)");
        printf("\n2. Exit");
        printf("\nEnter your choice : ");

        if (scanf("%d", &choice) != 1)
        {
            choice = 2; // Treat bad input as exit
        }
        flushInput(); // Clear newline after choice

        switch (choice)
        {
            case 1:
                runSenderProcess();
                printf("\n----------------------------------------\n");
                runReceiverProcess();
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

// --- Helper Functions ---

// Clears the input buffer to prevent issues with scanf
void flushInput()
{
    int c;
    while ((c = getchar()) != '\n' && c != EOF);
}

// Generic function to display any of the matrices
void displayMatrix(int matrix[ROWS + 1][COLS + 1], const char* title)
{
    printf("\n%s:\n\n", title);
    // Print column headers (1-indexed for user clarity)
    printf("     ");
    for (int j = 1; j <= COLS; j++) printf("C%d ", j);
    printf("RP\n"); // Row Parity header

    printf("   ");
    for (int j = 0; j <= COLS + 1; j++) printf("---");
    printf("\n");

    for (int i = 0; i <= ROWS; i++)
    {
        // Print row headers (1-indexed)
        if (i < ROWS) printf("R%d | ", i + 1);
        else printf("CP | "); // Column Parity row header

        for (int j = 0; j <= COLS; j++)
        {
            printf("%d  ", matrix[i][j]);
        }
        printf("\n");
    }
    printf("\n");
}

// --- Sender Implementation ---

void runSenderProcess()
{
    printf("\n========================================");
    printf("\n         2D PARITY - SENDER");
    printf("\n========================================\n");

    getDataSender();
    calculateParitySender();
    displayMatrix(dataMatrix, "Sender 2D Parity Matrix (with Parity)");
    writeToChannel();
}

void getDataSender()
{
    int i, j; // <--- FIXED: Declaration added back here
    printf("Enter %d x %d Binary Data (single bits separated by space/enter):\n", ROWS, COLS);

    for (i = 0; i < ROWS; i++)
    {
        for (j = 0; j < COLS; j++)
        {
            int input;
            do
            {
                if (scanf("%d", &input) != 1)
                {
                     // Handle non-numeric input
                     flushInput();
                     input = -1;
                }

                if (input != 0 && input != 1)
                {
                    printf("Invalid input (%d). Enter only 0 or 1: ", input);
                }
                else
                {
                    dataMatrix[i][j] = input;
                }
            }
            while (input != 0 && input != 1);
        }
    }
}

void calculateParitySender()
{
    int i, j, count;

    // 1. Calculate Row Parity (last column, indices 0 to ROWS-1)
    for (i = 0; i < ROWS; i++)
    {
        count = 0;
        for (j = 0; j < COLS; j++)
        {
            if (dataMatrix[i][j] == 1)
                count++;
        }
        // Use even parity: if count is odd, set parity bit to 1
        dataMatrix[i][COLS] = (count % 2 == 0) ? 0 : 1;
    }

    // 2. Calculate Column Parity (last row, indices 0 to COLS-1)
    for (j = 0; j < COLS; j++)
    {
        count = 0;
        for (i = 0; i < ROWS; i++)
        {
            if (dataMatrix[i][j] == 1)
                count++;
        }
        dataMatrix[ROWS][j] = (count % 2 == 0) ? 0 : 1;
    }

    // 3. Calculate Parity of the Parity Bits (bottom-right corner)
    count = 0;
    // Check the row parity bits we just calculated to find CP parity
    for (j = 0; j < COLS; j++)
    {
        if (dataMatrix[ROWS][j] == 1)
            count++;
    }
    // Alternatively, checking the column parity bits gives the same result
    dataMatrix[ROWS][COLS] = (count % 2 == 0) ? 0 : 1;
}

// Writes the final matrix to a file to simulate transmission
void writeToChannel()
{
    FILE *fp;
    int i, j;

    fp = fopen("channel.txt", "w");

    if (fp == NULL)
    {
        printf("\nError opening channel.txt for writing\n");
        return;
    }

    for (i = 0; i <= ROWS; i++)
    {
        for (j = 0; j <= COLS; j++)
        {
            fprintf(fp, "%d ", dataMatrix[i][j]);
        }
        fprintf(fp, "\n");
    }

    fclose(fp);

    printf("\nMatrix written to channel.txt successfully.\n");
}

// --- Receiver Implementation ---

void runReceiverProcess()
{
    printf("\n========================================");
    printf("\n        2D PARITY - RECEIVER");
    printf("\n========================================\n");

    readFromChannel();
    introduceErrorReceiver();
    displayMatrix(receivedMatrix, "Final Received Matrix (after potential error)");
    checkParityReceiver();
}

// Reads matrix from channel file
void readFromChannel()
{
    FILE *fp;
    int i, j;

    fp = fopen("channel.txt", "r");

    if (fp == NULL)
    {
        printf("\nError: channel.txt not found. Run sender first.\n");
        exit(1); // Exit if no file exists
    }

    for (i = 0; i <= ROWS; i++)
    {
        for (j = 0; j <= COLS; j++)
        {
            if (fscanf(fp, "%d", &receivedMatrix[i][j]) != 1)
            {
                printf("Error reading channel.txt\n");
                fclose(fp);
                exit(1);
            }
        }
    }

    fclose(fp);

    displayMatrix(receivedMatrix, "Matrix received from channel");
}

// Allows user to simulate transmission noise by flipping a bit
void introduceErrorReceiver()
{
    int choice, row, col;

    printf("\nDo you want to introduce an error manually? (1=Yes, 2=No) : ");
    if(scanf("%d", &choice) != 1) choice = 2;
    flushInput();

    if (choice == 1)
    {
        printf("Enter Row (1-%d): ", ROWS);
        scanf("%d", &row);
        printf("Enter Column (1-%d): ", COLS);
        scanf("%d", &col);
        flushInput();

        // Convert to 0-indexed
        row--;
        col--;

        if (row < 0 || row >= ROWS || col < 0 || col >= COLS)
        {
            printf("Invalid Position. Cannot introduce error.\n");
            return;
        }

        // Flip the bit (0 becomes 1, 1 becomes 0)
        receivedMatrix[row][col] = (receivedMatrix[row][col] == 0) ? 1 : 0;
        printf("Bit at Row %d, Col %d flipped.\n", row + 1, col + 1);
    }
}

// Calculates syndromes to detect and correct errors
void checkParityReceiver()
{
    int i, j, count;
    int rowError = -1;
    int colError = -1;
    int rowErrorCount = 0;
    int colErrorCount = 0;

    // 1. Check Row Parity
    for (i = 0; i < ROWS; i++)
    {
        count = 0;
        for (j = 0; j < COLS; j++)
        {
            if (receivedMatrix[i][j] == 1)
                count++;
        }

        // Compare calculated parity with received parity bit
        if ((count % 2) != receivedMatrix[i][COLS])
        {
            rowError = i; // Store index of erroneous row
            rowErrorCount++;
        }
    }

    // 2. Check Column Parity
    for (j = 0; j < COLS; j++)
    {
        count = 0;
        for (i = 0; i < ROWS; i++)
        {
            if (receivedMatrix[i][j] == 1)
                count++;
        }

        // Compare calculated parity with received parity bit
        if ((count % 2) != receivedMatrix[ROWS][j])
        {
            colError = j; // Store index of erroneous column
            colErrorCount++;
        }
    }

    // 3. Analyze Syndromes
    if (rowErrorCount == 0 && colErrorCount == 0)
    {
        printf("\nResult: Data received without error.\n");
    }
    else if (rowErrorCount == 1 && colErrorCount == 1)
    {
        // Single bit error implies intersection of exactly one bad row and one bad col
        printf("\nResult: Single Bit Error Detected.\n");
        printf("Error Position : Row %d, Column %d \n", rowError + 1, colError + 1);

        // Correct the bit by flipping it back
        receivedMatrix[rowError][colError] = (receivedMatrix[rowError][colError] == 0) ? 1 : 0;

        printf("Error Corrected Successfully.\n");
        displayMatrix(receivedMatrix, "Corrected Matrix");
    }
    else
    {
        // Two or more bit errors in specific patterns result in >1 row/col errors.
        // 2D parity can detect these but not correct them.
        printf("\nResult: Burst Error or Multiple Bit Errors Detected.\n");
        printf("Error cannot be corrected.\n");
    }
    
}
