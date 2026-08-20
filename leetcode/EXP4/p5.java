import java.util.*;

class Solution {

    public int minReorder(int n, int[][] connections) {

        List<int[]>[] graph = new ArrayList[n];

        for (int i = 0; i < n; i++) {
            graph[i] = new ArrayList<>();
        }

        // Build graph
        for (int[] edge : connections) {
            int from = edge[0];
            int to = edge[1];

            graph[from].add(new int[]{to, 1}); // original direction
            graph[to].add(new int[]{from, 0}); // reverse direction
        }

        boolean[] visited = new boolean[n];

        return dfs(0, graph, visited);
    }

    private int dfs(int city, List<int[]>[] graph, boolean[] visited) {

        visited[city] = true;

        int changes = 0;

        for (int[] neighbor : graph[city]) {

            int nextCity = neighbor[0];
            int needsReverse = neighbor[1];

            if (!visited[nextCity]) {
                changes += needsReverse;
                changes += dfs(nextCity, graph, visited);
            }
        }

        return changes;
    }
}
