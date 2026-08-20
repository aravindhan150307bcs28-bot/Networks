import java.util.*;

public class Main {
    public static void main(String[] args) {
        Scanner sc = new Scanner(System.in);

        int T = sc.nextInt();

        while (T-- > 0) {
            String s = sc.next();
            int sum = 0;

            for (int i = 0; i < s.length(); i++) {
                char ch = s.charAt(i);

                if (Character.isDigit(ch)) {
                    sum += ch - '0';
                }
            }

            System.out.println(sum);
        }

        sc.close();
    }
}
