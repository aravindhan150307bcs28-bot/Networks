import java.util.*;

class Solution {
    public int subarrayBitwiseORs(int[] arr) {
        Set<Integer> result = new HashSet<>();
        Set<Integer> prev = new HashSet<>();

        for (int num : arr) {
            Set<Integer> curr = new HashSet<>();

            // Start a new subarray
            curr.add(num);

            // Extend previous subarrays
            for (int val : prev) {
                curr.add(val | num);
            }

            // Add all current OR values to result
            result.addAll(curr);

            // Update previous set
            prev = curr;
        }

        return result.size();
    }
}
