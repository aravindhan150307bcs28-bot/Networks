import java.util.*;

class Solution {

    class Node {
        int row;
        int col;
        int effort;

        Node(int row, int col, int effort) {
            this.row = row;
            this.col = col;
            this.effort = effort;
        }
    }

    public int minimumEffortPath(int[][] heights) {

        int rows = heights.length;
        int cols = heights[0].length;

        int[][] effort = new int[rows][cols];

        for (int[] arr : effort) {
            Arrays.fill(arr, Integer.MAX_VALUE);
        }

        effort[0][0] = 0;

        PriorityQueue<Node> pq =
                new PriorityQueue<>((a, b) -> a.effort - b.effort);

        pq.offer(new Node(0, 0, 0));

        int[] dr = {-1, 1, 0, 0};
        int[] dc = {0, 0, -1, 1};

        while (!pq.isEmpty()) {

            Node curr = pq.poll();

            int r = curr.row;
            int c = curr.col;
            int currEffort = curr.effort;

            if (r == rows - 1 && c == cols - 1) {
                return currEffort;
            }

            for (int i = 0; i < 4; i++) {

                int nr = r + dr[i];
                int nc = c + dc[i];

                if (nr >= 0 && nr < rows && nc >= 0 && nc < cols) {

                    int diff = Math.abs(heights[r][c] - heights[nr][nc]);

                    int newEffort = Math.max(currEffort, diff);

                    if (newEffort < effort[nr][nc]) {
                        effort[nr][nc] = newEffort;
                        pq.offer(new Node(nr, nc, newEffort));
                    }
                }
            }
        }

        return 0;
    }
}
