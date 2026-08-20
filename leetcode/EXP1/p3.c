import java.util.HashSet;
import java.util.Set;

public class Solution {
    public int countPalindromicSubsequence(String s) {
        int n = s.length();
        int totalUniquePalindromes = 0;

        // Iterate through every lowercase letter 'a' to 'z'
        for (char ch = 'a'; ch <= 'z'; ch++) {
            // Find the first and last occurrence of the character
            int first = s.indexOf(ch);
            int last = s.lastIndexOf(ch);

            // If the character appears at least twice, it can form the 
            // outer parts of a length-3 palindrome (e.g., "a...a")
            if (first != -1 && last != -1 && last > first + 1) {
                // Use a Set to count unique characters found between
                // the first and last indices.
                Set<Character> middleChars = new HashSet<>();
                
                // Iterate only between the boundaries
                for (int i = first + 1; i < last; i++) {
                    middleChars.add(s.charAt(i));
                }
                
                // The size of the set equals the number of unique middle characters,
                // which determines the number of unique palindromes for this outer character.
                totalUniquePalindromes += middleChars.size();
            }
        }

        return totalUniquePalindromes;
    }
}
