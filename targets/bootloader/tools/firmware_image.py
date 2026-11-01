"""Image descriptor and combined image for a firmware that runs behind the e-foc bootloader.

stamp    computes the application's image descriptor (size and SHA-256 of the image as a debugger
         programs it) and writes it into the ELF's .image_descriptor section, so the bootloader starts
         an application that was programmed with a debugger.
combine  joins the bootloader's and the application's Intel HEX files into one file a debugger programs
         in one go, starting at the bootloader.

The layout is documented in documentation/design/bootloader.md, Parts A and E.
"""

import argparse
import hashlib
import pathlib
import struct
import subprocess
import tempfile

DESCRIPTOR_SECTION = ".image_descriptor"
DESCRIPTOR_MAGIC = 0x44494645
DESCRIPTOR_SIZE = 64


def image_bytes(objcopy, elf, directory):
    binary = pathlib.Path(directory) / "image.bin"
    subprocess.run([objcopy, "-O", "binary", f"--remove-section={DESCRIPTOR_SECTION}", "--gap-fill", "0xff", str(elf), str(binary)], check=True)
    return binary.read_bytes()


def descriptor(image):
    body = struct.pack("<II", DESCRIPTOR_MAGIC, len(image)) + hashlib.sha256(image).digest()
    return body + b"\xff" * (DESCRIPTOR_SIZE - len(body))


def stamp(arguments):
    with tempfile.TemporaryDirectory() as directory:
        image = image_bytes(arguments.objcopy, arguments.elf, directory)
        section = pathlib.Path(directory) / "descriptor.bin"
        section.write_bytes(descriptor(image))
        subprocess.run([arguments.objcopy, "--update-section", f"{DESCRIPTOR_SECTION}={section}", str(arguments.elf)], check=True)
    print(f"{arguments.elf}: image descriptor for {len(image)} bytes")


def records(path, keep_start_address):
    for line in pathlib.Path(path).read_text(encoding="ascii").splitlines():
        line = line.strip()
        if not line:
            continue
        record_type = line[7:9]
        if record_type == "01":
            continue
        if record_type in ("03", "05") and not keep_start_address:
            continue
        yield line


def combine(arguments):
    lines = list(records(arguments.bootloader, keep_start_address=True))
    lines.extend([":020000020000FC", ":020000040000FA"])
    lines.extend(records(arguments.application, keep_start_address=False))
    lines.append(":00000001FF")
    pathlib.Path(arguments.output).write_text("\n".join(lines) + "\n", encoding="ascii")
    print(f"{arguments.output}: {arguments.bootloader} and {arguments.application}")


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    commands = parser.add_subparsers(required=True)

    stamp_parser = commands.add_parser("stamp", help="write the image descriptor into an application ELF")
    stamp_parser.add_argument("--objcopy", required=True)
    stamp_parser.add_argument("elf", type=pathlib.Path)
    stamp_parser.set_defaults(run=stamp)

    combine_parser = commands.add_parser("combine", help="join the bootloader and application HEX files")
    combine_parser.add_argument("--output", required=True)
    combine_parser.add_argument("bootloader")
    combine_parser.add_argument("application")
    combine_parser.set_defaults(run=combine)

    arguments = parser.parse_args()
    arguments.run(arguments)


if __name__ == "__main__":
    main()
