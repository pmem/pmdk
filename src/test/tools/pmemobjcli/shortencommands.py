import sys
import re
import os.path


def validate_input():
    if len(sys.argv) > 1:
        if not os.path.isfile(sys.argv[1]):
            print("Error: pmemobjcli file %s does not exist." % sys.argv[1])
            return False
        if len(sys.argv) > 2:
            if not os.path.isfile(sys.argv[2]):
                print("Error: input file %s does not exist." % sys.argv[2])
                return False
        else:
            print("Error: Input file path empty. Please enter a correct path to input file.")
            return False
    else:
        print_help()
        return False

    return True


def pmem_obj_cli_functions_block():
    lines = []
    pmem_obj_cli_file = sys.argv[1]
    pattern_of_beginning = re.compile("(pocli_cmd pocli_commands)")
    pattern_of_end = re.compile("(};)")
    start_appending = False
    start_of_command_block = -1
    end_of_command_block = -1
    for i, line in enumerate(open(pmem_obj_cli_file)):
        for _ in re.finditer(pattern_of_beginning, line):
            start_of_command_block = i
            start_appending = True
        if start_appending:
            lines.append(line)
            for _ in re.finditer(pattern_of_end, line):
                end_of_command_block = i
        if start_of_command_block > 0 and end_of_command_block > 0:
            print("start: ", start_of_command_block)
            print("end: ", end_of_command_block)
            break
    return lines


def shortened_command_names(lines):
    dictionary = {}
    pattern = re.compile("(^(?!static struct pocli_cmd pocli_commands))(.*{.*)")

    pattern2 = re.compile("\",.*")
    for i, line in enumerate(lines):
        for match in re.finditer(pattern, line):
            long_name = lines[i+1].strip()
            long_name = re.sub(pattern2, "", long_name)
            long_name = long_name.replace("\"", "")

            short_name = lines[i + 2].strip()
            short_name = re.sub(pattern2, "", short_name)
            short_name = short_name.replace("\"", "")

            dictionary[long_name] = short_name

    return dictionary


def parse_input_file(dictionary):
    input_file = sys.argv[2]
    lines = []
    for i, line in enumerate(open(input_file)):
        splited_line = line.split()
        if len(splited_line) > 0:
            word = splited_line[0]
            if word in dictionary:
                line = line.replace(word, dictionary[word])
        lines.append(line)
    return lines


def write_lines_to_output(lines, output_file_path):
    output_file = open(output_file_path, 'w')
    output_file.writelines(lines)
    output_file.close()
    print("Output written to : " + os.path.abspath(output_file.name))


def print_help():
    print("Script used to get commands from pmemobjcli.c that translates them into their shorter counterparts.\r\n"
          "At least 2 parameters needed.\r\n"
          "Parameters:\r\n"
          "1st parameter: path to the pmemobjcli file\r\n"
          "2nd parameter: path to the input file\r\n"
          "3rd parameter: destination path of output file. Saved as \"output\" in current directory by default.")


def main():
    if len(sys.argv) > 1:
        if (sys.argv[1] == "-h") or (sys.argv[1] == "--help"):
            print_help()
        elif validate_input():
            print("Path to pmemobj-cli.c file: " + sys.argv[1])
            lines = pmem_obj_cli_functions_block()
            shortened_commands_dictionary = shortened_command_names(lines)
            print("Path to input file: " + sys.argv[2])
            output = parse_input_file(shortened_commands_dictionary)
            if len(sys.argv) > 3:
                write_lines_to_output(output, sys.argv[3])
            else:
                write_lines_to_output(output, "output")
    else:
        print_help()


if __name__ == '__main__':
    main()
