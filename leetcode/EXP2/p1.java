class Solution {
    public String addBinary(String a, String b) {
        StringBuilder result = new StringBuilder();
        
        // Pointers for both strings starting at the last character
        int i = a.length() - 1;
        int j = b.length() - 1;
        int carry = 0;

        // Loop as long as there are digits to process or a carry remains
        while (i >= 0 || j >= 0 || carry > 0) {
            int sum = carry; // Start with the carry from the previous step

            // Add digit from string 'a' if within bounds
            if (i >= 0) {
                sum += a.charAt(i) - '0'; // Convert char to integer ('1' -> 1, '0' -> 0)
                i--;
            }

            // Add digit from string 'b' if within bounds
            if (j >= 0) {
                sum += b.charAt(j) - '0'; // Convert char to integer
                j--;
            }

            // The bit to append is sum % 2 (remainder)
            result.append(sum % 2);

            // Calculate the new carry (integer division by 2)
            carry = sum / 2;
        }

        // Since we added from right to left, the result is backward
        return result.reverse().toString();
    }
}
