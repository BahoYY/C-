#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <json-c/json.h>
#include <libxml/parser.h>
#include <libxml/tree.h>

//struct for holding the data from csv
struct SmartLogEntry {
    char device_id[8];
    char timestamp[20];
    float temperature;
    int humidity;
    char status[16];
    char location[31];
    char alert_level[10];
    int battery;
    char firmware_ver[20];
    int event_code;
};

//removing line endings for the proper Operation System
void remove_line_ending(char* line, int opsys) {
    size_t len = strlen(line);
    if (len == 0) return;

    if (opsys == 1) { // Windows (\r\n)
        if (len >= 2 && line[len - 2] == '\r' && line[len - 1] == '\n') {
            line[len - 2] = '\0'; 
        } else if (line[len - 1] == '\n') {
            line[len - 1] = '\0'; 
        }
    }
    else if (opsys == 2) { // Linux (\n)
        if (line[len - 1] == '\n') {
            line[len - 1] = '\0';
        }
    }
    else if (opsys == 3) { // MacOS klasik (\r)
        if (line[len - 1] == '\r') {
            line[len - 1] = '\0';
        }
    }
}

// Global variables for setup parameters from JSON
char dataFileName[256];
int keyStart, keyEnd;
char order[10];

// Function to read configuration from a JSON file
void readSetupParams(const char* json_file_path) {
    FILE* fp = fopen(json_file_path, "r");
    if (!fp) {
        perror("setupParams.json open error");
        exit(1);
    }

    fseek(fp, 0, SEEK_END);
    long len = ftell(fp);
    rewind(fp);

    char* data = malloc(len + 1);
    fread(data, 1, len, fp);
    fclose(fp);
    data[len] = '\0';

    struct json_object *parsed_json = json_tokener_parse(data);
    struct json_object *j_dataFileName, *j_keyStart, *j_keyEnd, *j_order;

    // Extract required fields from JSON
    json_object_object_get_ex(parsed_json, "dataFileName", &j_dataFileName);
    json_object_object_get_ex(parsed_json, "keyStart", &j_keyStart);
    json_object_object_get_ex(parsed_json, "keyEnd", &j_keyEnd);
    json_object_object_get_ex(parsed_json, "order", &j_order);

    // Copy the values to global variables
    strcpy(dataFileName, json_object_get_string(j_dataFileName));
    keyStart = json_object_get_int(j_keyStart);
    keyEnd = json_object_get_int(j_keyEnd);
    strcpy(order, json_object_get_string(j_order));

    free(data);
    json_object_put(parsed_json);
}

// Global array to hold entries from binary file
struct SmartLogEntry entries[1000];
int entry_count = 0;

// Function to read binary file into the entries array
void readBinaryFile(const char* bin_file_path) {
    FILE* bin_fp = fopen(bin_file_path, "rb");
    if (!bin_fp) {
        perror("Binary file could not be opened");
        exit(1);
    }

    while (fread(&entries[entry_count], sizeof(struct SmartLogEntry), 1, bin_fp)) {
        entry_count++;
    }

    fclose(bin_fp);
}

// Comparison function for qsort
int compare_entries(const void* a, const void* b) {
    struct SmartLogEntry* e1 = (struct SmartLogEntry*)a;
    struct SmartLogEntry* e2 = (struct SmartLogEntry*)b;

    int cmp = strncmp(e1->device_id + keyStart, e2->device_id + keyStart, keyEnd - keyStart + 1);

    if (strcmp(order, "ASC") == 0)
        return cmp;
    else
        return -cmp;
}

// Function to write entries array into an XML file
void writeToXML(const char* output_xml_name) {
    xmlDocPtr doc = xmlNewDoc(BAD_CAST "1.0");// Create a new XML document
    xmlNodePtr root_node = xmlNewNode(NULL, BAD_CAST output_xml_name);// Create root node
    xmlDocSetRootElement(doc, root_node);
    

    for (int i = 0; i < entry_count; i++) {
        xmlNodePtr entry_node = xmlNewChild(root_node, NULL, BAD_CAST "entry", NULL);

        char id_str[10];
        sprintf(id_str, "%d", i+1);
        xmlNewProp(entry_node, BAD_CAST "id", BAD_CAST id_str);

        // Create child nodes for each field
        xmlNewChild(entry_node, NULL, BAD_CAST "device_id", BAD_CAST entries[i].device_id);
        xmlNewChild(entry_node, NULL, BAD_CAST "timestamp", BAD_CAST entries[i].timestamp);

        char temp_str[20];
        sprintf(temp_str, "%.2f", entries[i].temperature);
        xmlNewChild(entry_node, NULL, BAD_CAST "temperature", BAD_CAST temp_str);

        char hum_str[10];
        sprintf(hum_str, "%d", entries[i].humidity);
        xmlNewChild(entry_node, NULL, BAD_CAST "humidity", BAD_CAST hum_str);

        xmlNewChild(entry_node, NULL, BAD_CAST "status", BAD_CAST entries[i].status);
        xmlNewChild(entry_node, NULL, BAD_CAST "location", BAD_CAST entries[i].location);
        xmlNewChild(entry_node, NULL, BAD_CAST "alert_level", BAD_CAST entries[i].alert_level);

        char batt_str[10];
        sprintf(batt_str, "%d", entries[i].battery);
        xmlNewChild(entry_node, NULL, BAD_CAST "battery", BAD_CAST batt_str);

        xmlNewChild(entry_node, NULL, BAD_CAST "firmware_ver", BAD_CAST entries[i].firmware_ver);

        // Special handling for event_code with additional attributes
        char ev_code_str[10];
        sprintf(ev_code_str, "%d", entries[i].event_code);

        xmlNodePtr ev_node = xmlNewChild(entry_node, NULL, BAD_CAST "event_code", BAD_CAST ev_code_str);

        char hex_big[9];
        sprintf(hex_big, "%02X000000", entries[i].event_code);
        xmlNewProp(ev_node, BAD_CAST "hexBig", BAD_CAST hex_big);

        char hex_little[9];
        sprintf(hex_little, "000000%02X", entries[i].event_code);
        xmlNewProp(ev_node, BAD_CAST "hexLittle", BAD_CAST hex_little);

        char dec_from_little[10];
        sprintf(dec_from_little, "%d", entries[i].event_code);
        xmlNewProp(ev_node, BAD_CAST "decFromHexLittle", BAD_CAST dec_from_little);
    }

    xmlSaveFormatFileEnc(output_xml_name, doc, "UTF-8", 1);
    xmlFreeDoc(doc);
    xmlCleanupParser();
}
// Function to convert CSV to binary format
void convertCsvToBin(char* input_file, char* output_file, int seperator, int opsys) {
    FILE *input_csv, *output_bin;
    int ctr = 0;
    struct SmartLogEntry slog;
    input_csv = fopen(input_file, "r");
    
    //Error handling for input file
    if (input_csv == NULL) {
        printf("Error: Could not open CSV file.\n");
        exit(0);
    }

    //Error handling for output file
    output_bin = fopen(output_file, "wb");
    if (output_bin == NULL) {
        printf("Error: Could not create binary file.\n");
        exit(0);
    }
    //reading the csv file 
    char *line = calloc(1024, sizeof(char));
    while (fgets(line, 1024, input_csv)) {
        remove_line_ending(line,opsys);
        memset(&slog, 0, sizeof(struct SmartLogEntry));

        
        
        if (seperator == 1)//seperator for ','
        {
            if (ctr == 0) {
                ctr++;
                continue;
            }
            
            // getting device_id
            char *wrd = strtok(line, ",");
            if (wrd != NULL) {
                strcpy(slog.device_id, wrd);
            }
            // getting timestamp 
            wrd = strtok(NULL, ",");
            if (wrd != NULL) {
                strcpy(slog.timestamp, wrd);
            }
            //getting temperature
            wrd = strtok(NULL, ",");
            if (wrd != NULL && atoi(wrd) <= 60.00 && atoi(wrd) >= -30.00) {
                slog.temperature = atoi(wrd);
            }
            //getting humidity
            wrd = strtok(NULL, ",");
            if (wrd != NULL && atoi(wrd)<= 100 && atoi(wrd) >= 0) {
                slog.humidity =  atoi(wrd);
            }
            //getting status
            wrd = strtok(NULL, ",");
            if (wrd != NULL) {
                strcpy(slog.status, wrd);
            }
            //getting location 
            wrd = strtok(NULL, ",");
            if (wrd != NULL) {
                strcpy(slog.location, wrd);
            }
            // getting alert_level 
            wrd = strtok(NULL, ",");
            if (wrd != NULL) {
                strcpy(slog.alert_level, wrd);
            }
            // getting battery
            wrd = strtok(NULL, ",");
            if (wrd != NULL && atoi(wrd) <= 100 && atoi(wrd) >= 0) {
                slog.battery = atoi(wrd);
            }
            // getting firmware_ver
            wrd = strtok(NULL, ",");
            if (wrd != NULL) {
                strcpy(slog.firmware_ver, wrd);
            }
            // getting event_code
            wrd = strtok(NULL, ",");
            if (wrd != NULL && atoi(wrd) <= 255 && atoi(wrd) >= 0) {
                slog.event_code = atoi(wrd);
            }
        }
        else if (seperator == 2)//seperator for '\t'
        {
            if (ctr == 0) {
                ctr++;
                continue;
            }
            // getting device_id
            char *wrd = strtok(line, "\t");
            if (wrd != NULL) {
                strcpy(slog.device_id, wrd);
            }
            // getting timestamp
            wrd = strtok(NULL, "\t");
            if (wrd != NULL) {
                strcpy(slog.timestamp, wrd);
            }
            //getting temperature
            wrd = strtok(NULL, "\t");
            if (wrd != NULL && atoi(wrd) <= 60.00 && atoi(wrd) >= -30.00) {
                slog.temperature = atoi(wrd);
            }
             //getting humidity
            wrd = strtok(NULL, "\t");
            if (wrd != NULL && atoi(wrd) <= 100 && atoi(wrd) >= 0) {
                slog.humidity = atoi(wrd);
            }
            //getting status
            wrd = strtok(NULL, "\t");
            if (wrd != NULL) {
                strcpy(slog.status, wrd);
            }
            //getting location
            wrd = strtok(NULL, "\t");
            if (wrd != NULL) {
                strcpy(slog.location, wrd);
            }
             // getting alert_level
            wrd = strtok(NULL, "\t");
            if (wrd != NULL) {
                strcpy(slog.alert_level, wrd);
            }
            // getting battery
            wrd = strtok(NULL, "\t");
            if (wrd != NULL && atoi(wrd) <= 100 && atoi(wrd) >= 0) {
                slog.battery = atoi(wrd);
            }
            // getting firmware_ver
            wrd = strtok(NULL, "\t");
            if (wrd != NULL) {
                strcpy(slog.firmware_ver, wrd);
            }
            // getting event_code
            wrd = strtok(NULL, "\t");
            if (wrd != NULL && atoi(wrd) <= 255 && atoi(wrd) >= 0) {
                slog.event_code = atoi(wrd);
            }

        }
        else if (seperator == 3)
        {
            if (ctr == 0) {
                ctr++;
                continue;
            }
            // getting device_id
            char *wrd = strtok(line, ";");
            if (wrd != NULL) {
                strcpy(slog.device_id, wrd);
            }
            // getting timestamp
            wrd = strtok(NULL, ";");
            if (wrd != NULL) {
                strcpy(slog.timestamp, wrd);
            }
            //getting temperature
            wrd = strtok(NULL, ";");
            if (wrd != NULL && atoi(wrd) <= 60.00 && atoi(wrd) >= -30.00) {
                slog.temperature = atoi(wrd);
            }
             //getting humidity
            wrd = strtok(NULL, ";");
            if (wrd != NULL && atoi(wrd) <= 100 && atoi(wrd) >= 0) {
                slog.humidity = atoi(wrd);
            }
            //getting status
            wrd = strtok(NULL, ";");
            if (wrd != NULL) {
                strcpy(slog.status, wrd);
            }
            //getting location 
            wrd = strtok(NULL, ";");
            if (wrd != NULL) {
                strcpy(slog.location, wrd);
            }
             // getting alert_level
            wrd = strtok(NULL, ";");
            if (wrd != NULL) {
                strcpy(slog.alert_level, wrd);
            }
            // getting battery
            wrd = strtok(NULL, ";");
            if (wrd != NULL && atoi(wrd) <= 100 && atoi(wrd) >= 0) {
                slog.battery = atoi(wrd);
            }
            // getting firmware_ver
            wrd = strtok(NULL, ";");
            if (wrd != NULL) {
                strcpy(slog.firmware_ver, wrd);
            }
            // getting event_code
            wrd = strtok(NULL, ";");
            if (wrd != NULL && atoi(wrd) <= 255 && atoi(wrd) >= 0) {
                slog.event_code = atoi(wrd);
            }

        }
        

        fwrite(&slog, sizeof(struct SmartLogEntry), 1, output_bin);
        ctr++;
    }
    free(line);
    fclose(input_csv);
    fclose(output_bin);
    printf("binary file is created!!!\n");
}

//Function for converting binary file to xml file
void convertBinToXml(char* json_file, char* output_xml) {
    readSetupParams(json_file);
    readBinaryFile(dataFileName);
    qsort(entries, entry_count, sizeof(struct SmartLogEntry), compare_entries);
    writeToXML(output_xml);
}
//main
int main(int argc, char* argv[]) {
    //error handling for input
    if (argc != 6 && argc != 4) {
        printf("Usage: %s inputFile, outputFile, separator, opsys, conversionType or inputFile, outputFile, conversionType \n", argv[0]);
        return 1;
    }
    //error handling for conversion type
    int conversion_type;
    if (argc == 6)
    {
        conversion_type = atoi(argv[5]);
    }
    else if (argc = 4)
    {
        conversion_type = atoi(argv[3]);
    }
    
    

    
    //choosing conversion type
    switch (conversion_type) {
        case 1:
            convertCsvToBin(argv[1], argv[2], atoi(argv[3]), atoi(argv[4]));
            break;
        case 2:
            convertBinToXml(argv[1], argv[2]);
            break;
        default:
            printf("Invalid conversion type. Please choose 1 or 2.\n");
            return 1;
    }

    return 0;
}
