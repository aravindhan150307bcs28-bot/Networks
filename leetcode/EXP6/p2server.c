#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <time.h>
#include <ctype.h>

#define PORT 8080
#define MAX_CLIENTS 5
#define BUFFER_SIZE 2048
#define LOG_FILE "server_log.txt"
#define DOCTORS_FILE "doctors.dat"
#define PATIENTS_FILE "patients.dat"
#define APPOINTMENTS_FILE "appointments.dat"

#define MAX_DOCTORS 20
#define MAX_PATIENTS 100
#define MAX_APPOINTMENTS 500
#define MAX_SLOTS_PER_DOC 50

// ==========================================
// DATA STRUCTURES
// ==========================================
typedef struct {
    int slot_id;
    char date[11];      // YYYY-MM-DD
    char start_time[6]; // HH:MM
    char end_time[6];   // HH:MM
    int is_booked;
    int patient_id;     // -1 if free
} Slot;

typedef struct {
    int doctor_id;
    char name[50];
    char specialization[50];
    Slot schedule[MAX_SLOTS_PER_DOC];
    int slot_count;
} Doctor;

typedef struct {
    int patient_id;
    char name[50];
    char phone[15];
    char dob[11]; // YYYY-MM-DD
} Patient;

typedef struct {
    int app_id;
    int patient_id;
    int doctor_id;
    int slot_id;
    char booking_time[20]; // Timestamp of booking
    int status; // 1=Active, 0=Cancelled
} Appointment;

// ==========================================
// GLOBAL DATABASE (In-Memory)
// ==========================================
Doctor doctors[MAX_DOCTORS];
int doctor_count = 0;

Patient patients[MAX_PATIENTS];
int patient_count = 0;

Appointment appointments[MAX_APPOINTMENTS];
int appointment_count = 0;
int next_app_id = 1;

// ==========================================
// UTILITY & LOGGING
// ==========================================
void log_operation(const char *client_ip, const char *operation, const char *status, const char *details) {
    FILE *fp = fopen(LOG_FILE, "a");
    if (!fp) return;

    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    char timestamp[30];
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", t);

    fprintf(fp, "[%s] IP: %s | Op: %s | Status: %s | Details: %s\n",
            timestamp, client_ip, operation, status, details);
    fclose(fp);
}

void send_response(int client_sock, const char *status, const char *message) {
    char buffer[BUFFER_SIZE];
    snprintf(buffer, sizeof(buffer), "%s|%s\n", status, message);
    send(client_sock, buffer, strlen(buffer), 0);
}

int validate_date(const char *date) {
    // Basic YYYY-MM-DD check
    if (strlen(date) != 10) return 0;
    if (date[4] != '-' || date[7] != '-') return 0;
    for (int i=0; i<10; i++) if (i!=4 && i!=7 && !isdigit(date[i])) return 0;
    int y = atoi(date), m = atoi(date+5), d = atoi(date+8);
    if (m < 1 || m > 12 || d < 1 || d > 31) return 0;
    // Note: Real implementation needs day-per-month/leap year check
    return 1;
}

int validate_time(const char *time) {
    if (strlen(time) != 5) return 0;
    if (time[2] != ':') return 0;
    int h = atoi(time), m = atoi(time+3);
    return (h >= 0 && h <= 23 && m >= 0 && m <= 59);
}

// ==========================================
// FILE I/O (PERSISTENCE)
// ==========================================
void save_data() {
    FILE *fp;

    fp = fopen(DOCTORS_FILE, "wb");
    if (fp) { fwrite(&doctor_count, sizeof(int), 1, fp); fwrite(doctors, sizeof(Doctor), doctor_count, fp); fclose(fp); }

    fp = fopen(PATIENTS_FILE, "wb");
    if (fp) { fwrite(&patient_count, sizeof(int), 1, fp); fwrite(patients, sizeof(Patient), patient_count, fp); fclose(fp); }

    fp = fopen(APPOINTMENTS_FILE, "wb");
    if (fp) { fwrite(&appointment_count, sizeof(int), 1, fp); fwrite(&next_app_id, sizeof(int), 1, fp); fwrite(appointments, sizeof(Appointment), appointment_count, fp); fclose(fp); }
}

void load_data() {
    FILE *fp;

    fp = fopen(DOCTORS_FILE, "rb");
    if (fp) { fread(&doctor_count, sizeof(int), 1, fp); fread(doctors, sizeof(Doctor), doctor_count, fp); fclose(fp); }
    else { /* Initialize Default Doctors if file missing */
        doctor_count = 2;
        doctors[0] = (Doctor){1, "Dr. Arjun Sharma", "Cardiology", {{0}}, 0};
        doctors[1] = (Doctor){2, "Dr. Priya Patel", "Dermatology", {{0}}, 0};
        // Add default slots for demo
        strcpy(doctors[0].schedule[0].date, "2023-10-25"); strcpy(doctors[0].schedule[0].start_time, "10:00"); strcpy(doctors[0].schedule[0].end_time, "10:30"); doctors[0].schedule[0].slot_id=1; doctors[0].schedule[0].is_booked=0; doctors[0].schedule[0].patient_id=-1;
        strcpy(doctors[0].schedule[1].date, "2023-10-25"); strcpy(doctors[0].schedule[1].start_time, "10:30"); strcpy(doctors[0].schedule[1].end_time, "11:00"); doctors[0].schedule[1].slot_id=2; doctors[0].schedule[1].is_booked=0; doctors[0].schedule[1].patient_id=-1;
        doctors[0].slot_count = 2;

        strcpy(doctors[1].schedule[0].date, "2023-10-26"); strcpy(doctors[1].schedule[0].start_time, "14:00"); strcpy(doctors[1].schedule[0].end_time, "14:30"); doctors[1].schedule[0].slot_id=3; doctors[1].schedule[0].is_booked=0; doctors[1].schedule[0].patient_id=-1;
        doctors[1].slot_count = 1;
    }

    fp = fopen(PATIENTS_FILE, "rb");
    if (fp) { fread(&patient_count, sizeof(int), 1, fp); fread(patients, sizeof(Patient), patient_count, fp); fclose(fp); }

    fp = fopen(APPOINTMENTS_FILE, "rb");
    if (fp) { fread(&appointment_count, sizeof(int), 1, fp); fread(&next_app_id, sizeof(int), 1, fp); fread(appointments, sizeof(Appointment), appointment_count, fp); fclose(fp); }
}

// ==========================================
// CORE BUSINESS LOGIC
// ==========================================

// Helper: Find Doctor Index by ID
int find_doctor(int id) {
    for(int i=0; i<doctor_count; i++) if(doctors[i].doctor_id == id) return i;
    return -1;
}

// Helper: Find Patient Index by ID
int find_patient(int id) {
    for(int i=0; i<patient_count; i++) if(patients[i].patient_id == id) return i;
    return -1;
}

// Helper: Find Slot Index (Doctor Index, Slot Index)
void find_slot(int doctor_idx, int slot_id, int *slot_idx) {
    *slot_idx = -1;
    if(doctor_idx == -1) return;
    for(int i=0; i<doctors[doctor_idx].slot_count; i++) {
        if(doctors[doctor_idx].schedule[i].slot_id == slot_id) { *slot_idx = i; return; }
    }
}

// 1. REGISTER PATIENT
// CMD: REGISTER|Name|Phone|DOB
void handle_register(int client_sock, char *args, char *client_ip) {
    char *name = strtok(args, "|");
    char *phone = strtok(NULL, "|");
    char *dob = strtok(NULL, "|");

    if (!name || !phone || !dob || !validate_date(dob)) {
        send_response(client_sock, "ERROR", "Invalid arguments. Usage: REGISTER|Name|Phone|YYYY-MM-DD");
        log_operation(client_ip, "REGISTER", "FAIL", "Invalid input format");
        return;
    }
    if (patient_count >= MAX_PATIENTS) {
        send_response(client_sock, "ERROR", "Patient database full");
        return;
    }

    int new_id = (patient_count == 0) ? 1 : patients[patient_count-1].patient_id + 1;
    patients[patient_count] = (Patient){new_id, "", "", ""};
    strncpy(patients[patient_count].name, name, 49);
    strncpy(patients[patient_count].phone, phone, 14);
    strncpy(patients[patient_count].dob, dob, 10);
    patient_count++;
    save_data();

    char msg[100];
    sprintf(msg, "Patient registered successfully. Your Patient ID: %d", new_id);
    send_response(client_sock, "OK", msg);
    log_operation(client_ip, "REGISTER", "SUCCESS", msg);
}

// 2. ADD DOCTOR SLOT (Admin/Server side utility, exposed for demo)
// CMD: ADDSLOT|DocID|Date|StartTime|EndTime
void handle_add_slot(int client_sock, char *args, char *client_ip) {
    char *doc_id_s = strtok(args, "|");
    char *date = strtok(NULL, "|");
    char *start = strtok(NULL, "|");
    char *end = strtok(NULL, "|");

    if(!doc_id_s || !date || !start || !end || !validate_date(date) || !validate_time(start) || !validate_time(end)) {
        send_response(client_sock, "ERROR", "Invalid args. Usage: ADDSLOT|DocID|YYYY-MM-DD|HH:MM|HH:MM");
        return;
    }
    int doc_id = atoi(doc_id_s);
    int d_idx = find_doctor(doc_id);
    if(d_idx == -1) { send_response(client_sock, "ERROR", "Doctor not found"); return; }
    if(doctors[d_idx].slot_count >= MAX_SLOTS_PER_DOC) { send_response(client_sock, "ERROR", "Doctor schedule full"); return; }

    Slot new_slot;
    new_slot.slot_id = (doctors[d_idx].slot_count == 0) ? 1 : doctors[d_idx].schedule[doctors[d_idx].slot_count-1].slot_id + 1;
    strcpy(new_slot.date, date);
    strcpy(new_slot.start_time, start);
    strcpy(new_slot.end_time, end);
    new_slot.is_booked = 0;
    new_slot.patient_id = -1;

    doctors[d_idx].schedule[doctors[d_idx].slot_count++] = new_slot;
    save_data();
    send_response(client_sock, "OK", "Slot added successfully");
    log_operation(client_ip, "ADDSLOT", "SUCCESS", args);
}

// 3. BOOK APPOINTMENT
// CMD: BOOK|PatientID|DoctorID|SlotID
void handle_book(int client_sock, char *args, char *client_ip) {
    char *pid_s = strtok(args, "|");
    char *did_s = strtok(NULL, "|");
    char *sid_s = strtok(NULL, "|");

    if(!pid_s || !did_s || !sid_s) { send_response(client_sock, "ERROR", "Invalid args. Usage: BOOK|PID|DID|SID"); return; }

    int pid = atoi(pid_s), did = atoi(did_s), sid = atoi(sid_s);
    int p_idx = find_patient(pid);
    int d_idx = find_doctor(did);
    int s_idx;

    if(p_idx == -1) { send_response(client_sock, "ERROR", "Patient not found"); log_operation(client_ip, "BOOK", "FAIL", "Patient not found"); return; }
    if(d_idx == -1) { send_response(client_sock, "ERROR", "Doctor not found"); log_operation(client_ip, "BOOK", "FAIL", "Doctor not found"); return; }

    find_slot(d_idx, sid, &s_idx);
    if(s_idx == -1) { send_response(client_sock, "ERROR", "Slot not found for this doctor"); log_operation(client_ip, "BOOK", "FAIL", "Slot not found"); return; }

    Slot *slot = &doctors[d_idx].schedule[s_idx];
    if(slot->is_booked) { send_response(client_sock, "ERROR", "Slot already booked"); log_operation(client_ip, "BOOK", "FAIL", "Slot taken"); return; }

    // BOOK IT
    slot->is_booked = 1;
    slot->patient_id = pid;

    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    strftime(appointments[appointment_count].booking_time, 20, "%Y-%m-%d %H:%M:%S", t);

    appointments[appointment_count] = (Appointment){
        next_app_id++, pid, did, sid, "", 1
    };
    strcpy(appointments[appointment_count].booking_time, appointments[appointment_count].booking_time); // already set above
    appointment_count++;
    save_data();

    char msg[200];
    sprintf(msg, "Appointment Confirmed! AppID: %d | Dr: %s | Date: %s | Time: %s-%s",
            appointments[appointment_count-1].app_id, doctors[d_idx].name, slot->date, slot->start_time, slot->end_time);
    send_response(client_sock, "OK", msg);
    log_operation(client_ip, "BOOK", "SUCCESS", msg);
}

// 4. CANCEL APPOINTMENT
// CMD: CANCEL|AppID|PatientID (Security: Patient must own appointment)
void handle_cancel(int client_sock, char *args, char *client_ip) {
    char *aid_s = strtok(args, "|");
    char *pid_s = strtok(NULL, "|");
    if(!aid_s || !pid_s) { send_response(client_sock, "ERROR", "Usage: CANCEL|AppID|PatientID"); return; }

    int aid = atoi(aid_s), pid = atoi(pid_s);
    int app_idx = -1;
    for(int i=0; i<appointment_count; i++) if(appointments[i].app_id == aid) { app_idx = i; break; }

    if(app_idx == -1) { send_response(client_sock, "ERROR", "Appointment not found"); return; }
    if(appointments[app_idx].patient_id != pid) { send_response(client_sock, "ERROR", "Unauthorized: Patient ID mismatch"); return; }
    if(appointments[app_idx].status == 0) { send_response(client_sock, "ERROR", "Already cancelled"); return; }

    appointments[app_idx].status = 0;

    // Free the slot
    int d_idx = find_doctor(appointments[app_idx].doctor_id);
    int s_idx;
    find_slot(d_idx, appointments[app_idx].slot_id, &s_idx);
    if(d_idx != -1 && s_idx != -1) {
        doctors[d_idx].schedule[s_idx].is_booked = 0;
        doctors[d_idx].schedule[s_idx].patient_id = -1;
    }
    save_data();
    send_response(client_sock, "OK", "Appointment cancelled successfully. Slot released.");
    log_operation(client_ip, "CANCEL", "SUCCESS", args);
}

// 5. SEARCH DOCTOR AVAILABILITY
// CMD: SEARCH|DoctorID|Date (Date optional)
void handle_search(int client_sock, char *args, char *client_ip) {
    char *did_s = strtok(args, "|");
    char *date = strtok(NULL, "|"); // Optional

    if(!did_s) { send_response(client_sock, "ERROR", "Usage: SEARCH|DoctorID|[Date]"); return; }
    int did = atoi(did_s);
    int d_idx = find_doctor(did);
    if(d_idx == -1) { send_response(client_sock, "ERROR", "Doctor not found"); return; }

    char response[BUFFER_SIZE] = "OK|";
    char temp[200];
    int found = 0;
    sprintf(temp, "Dr: %s (%s)\n", doctors[d_idx].name, doctors[d_idx].specialization);
    strcat(response, temp);

    for(int i=0; i<doctors[d_idx].slot_count; i++) {
        Slot *s = &doctors[d_idx].schedule[i];
        if(date && strcmp(s->date, date) != 0) continue;
        if(!s->is_booked) {
            found = 1;
            sprintf(temp, "SlotID: %d | Date: %s | Time: %s-%s [AVAILABLE]\n", s->slot_id, s->date, s->start_time, s->end_time);
            strcat(response, temp);
        }
    }
    if(!found) strcat(response, "No available slots found for the given criteria.\n");
    send(client_sock, response, strlen(response), 0);
    log_operation(client_ip, "SEARCH", "SUCCESS", args);
}

// 6. VIEW APPOINTMENT HISTORY
// CMD: HISTORY|PatientID
void handle_history(int client_sock, char *args, char *client_ip) {
    char *pid_s = strtok(args, "|");
    if(!pid_s) { send_response(client_sock, "ERROR", "Usage: HISTORY|PatientID"); return; }
    int pid = atoi(pid_s);
    if(find_patient(pid) == -1) { send_response(client_sock, "ERROR", "Patient not found"); return; }

    char response[BUFFER_SIZE] = "OK|";
    char temp[300];
    int found = 0;

    for(int i=0; i<appointment_count; i++) {
        if(appointments[i].patient_id == pid) {
            found = 1;
            int d_idx = find_doctor(appointments[i].doctor_id);
            char *dname = (d_idx != -1) ? doctors[d_idx].name : "Unknown";
            char *stat = appointments[i].status ? "ACTIVE" : "CANCELLED";

            // Find slot details for time/date
            char slot_info[50] = "N/A";
            if(d_idx != -1) {
                int s_idx; find_slot(d_idx, appointments[i].slot_id, &s_idx);
                if(s_idx != -1) {
                    Slot *sl = &doctors[d_idx].schedule[s_idx];
                    sprintf(slot_info, "%s %s-%s", sl->date, sl->start_time, sl->end_time);
                }
            }
            sprintf(temp, "AppID: %d | Dr: %s | Slot: %s | Booked: %s | Status: %s\n",
                    appointments[i].app_id, dname, slot_info, appointments[i].booking_time, stat);
            strcat(response, temp);
        }
    }
    if(!found) strcat(response, "No appointment history found.\n");
    send(client_sock, response, strlen(response), 0);
    log_operation(client_ip, "HISTORY", "SUCCESS", args);
}

// ==========================================
// COMMAND DISPATCHER
// ==========================================
void process_client(int client_sock, struct sockaddr_in *client_addr) {
    char buffer[BUFFER_SIZE];
    char client_ip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &client_addr->sin_addr, client_ip, INET_ADDRSTRLEN);

    // Iterative server reads one command per connection (or loop for multiple cmds per connection)
    // Requirement: "process one client's complete request before serving the next"
    // We will loop reading commands until client sends QUIT or disconnects.

    while(1) {
        memset(buffer, 0, BUFFER_SIZE);
        int bytes = recv(client_sock, buffer, BUFFER_SIZE - 1, 0);
        if(bytes <= 0) break; // Disconnect or error

        buffer[bytes] = '\0';
        // Trim newline
        buffer[strcspn(buffer, "\n")] = 0;
        buffer[strcspn(buffer, "\r")] = 0;

        if(strlen(buffer) == 0) continue;

        char *cmd = strtok(buffer, "|");
        char *args = strtok(NULL, ""); // Rest of string

        if(!cmd) { send_response(client_sock, "ERROR", "Empty command"); continue; }

        if(strcmp(cmd, "REGISTER") == 0) handle_register(client_sock, args ? args : "", client_ip);
        else if(strcmp(cmd, "ADDSLOT") == 0) handle_add_slot(client_sock, args ? args : "", client_ip);
        else if(strcmp(cmd, "BOOK") == 0) handle_book(client_sock, args ? args : "", client_ip);
        else if(strcmp(cmd, "CANCEL") == 0) handle_cancel(client_sock, args ? args : "", client_ip);
        else if(strcmp(cmd, "SEARCH") == 0) handle_search(client_sock, args ? args : "", client_ip);
        else if(strcmp(cmd, "HISTORY") == 0) handle_history(client_sock, args ? args : "", client_ip);
        else if(strcmp(cmd, "QUIT") == 0) { send_response(client_sock, "OK", "Goodbye"); break; }
        else { send_response(client_sock, "ERROR", "Unknown command"); log_operation(client_ip, cmd, "FAIL", "Unknown command"); }
    }
}

// ==========================================
// MAIN
// ==========================================
int main() {
    int server_fd, client_sock;
    struct sockaddr_in address, client_addr;
    int opt = 1;
    socklen_t addrlen = sizeof(client_addr);

    load_data(); // Load persistent state

    // Socket Creation
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) { perror("socket failed"); exit(EXIT_FAILURE); }
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt))) { perror("setsockopt"); exit(EXIT_FAILURE); }

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) { perror("bind failed"); exit(EXIT_FAILURE); }
    if (listen(server_fd, MAX_CLIENTS) < 0) { perror("listen"); exit(EXIT_FAILURE); }

    printf(">>> Hospital Appointment Server Started on Port %d <<<\n", PORT);
    printf(">>> Iterative Mode: Serving one client fully at a time. <<<\n");
    printf(">>> Logs writing to: %s <<<\n", LOG_FILE);

    while(1) {
        printf("\n[Server] Waiting for connection...\n");
        client_sock = accept(server_fd, (struct sockaddr *)&client_addr, &addrlen);
        if (client_sock < 0) { perror("accept"); continue; }

        char client_ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, INET_ADDRSTRLEN);
        printf("[Server] Accepted connection from %s:%d\n", client_ip, ntohs(client_addr.sin_port));

        // ITERATIVE PROCESSING: Blocks here until client disconnects
        process_client(client_sock, &client_addr);

        close(client_sock);
        printf("[Server] Connection closed. Ready for next client.\n");
    }

    close(server_fd);
    save_data(); // Final save on shutdown (Ctrl+C won't reach this easily, but good practice)
    return 0;
}
