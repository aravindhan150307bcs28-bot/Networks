class Solution {
    public boolean hasAlternatingBits(int n) {
        // Get the very first bit (rightmost bit)
        int lastBit = n % 2;
        n = n / 2; // Move to the next bit

        // Process the rest of the bits
        while (n > 0) {
            int currentBit = n % 2;

            // If adjacent bits are the same, it's not alternating
            if (currentBit == lastBit) {
                return false;
            }

            // Update lastBit for the next check, and move to the next bit
            lastBit = currentBit;
            n = n / 2;
        }

        return true;
    }
}
