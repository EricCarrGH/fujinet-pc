#include <string.h>
#include <cstdarg>
// #include <esp_system.h>
// #include <driver/uart.h>

#include <stdio.h>
#include "compat_string.h"

#include <sys/time.h>
#include <unistd.h> // write(), read(), close()
#include <errno.h> // Error integer and strerror() function

#if defined(_WIN32)
#else
#include <termios.h> // Contains POSIX terminal control definitions
#include <sys/ioctl.h> // TIOCM_DSR etc.
#endif

#include <fcntl.h> // Contains file controls like O_RDWR

#if defined(__linux__)
#include <linux/serial.h>
#include "linux_termios2.h"
#elif defined(__APPLE__)
#include <IOKit/serial/ioss.h>
#endif

#include "fnSystem.h"
#include "fnUART.h"
#include "../../include/debug.h"

#define UART_PROBE_DEV1 "/dev/ttyUSB0"
#define UART_PROBE_DEV2 "/dev/ttyS0"
#define UART_DEFAULT_BAUD 19200

#define UART_DEBUG UART_NUM_0
#define UART_ADAMNET UART_NUM_2
#ifdef BUILD_RS232
#define UART_SIO UART_NUM_1
#else
#define UART_SIO UART_NUM_2
#endif

// Number of RTOS ticks to wait for data in TX buffer to complete sending
#define MAX_FLUSH_WAIT_TICKS 200
#define MAX_READ_WAIT_TICKS 200
#define MAX_WRITE_BYTE_TICKS 100
#define MAX_WRITE_BUFFER_TICKS 1000

// UARTManager fnUartDebug(UART_DEBUG);
// UARTManager fnUartSIO;

// Constructor
// UARTManager::UARTManager(uart_port_t uart_num) : _uart_num(uart_num), _uart_q(NULL) {}
UARTManager::UARTManager() :
    _initialized(false),
    _fd(-1),
    _device{0},
    _baud(UART_DEFAULT_BAUD)
{};

void UARTManager::end()
{
    if (_fd >= 0)
    {
        close(_fd);
        _fd  = -1;
        Debug_printf("### UART stopped ###\n");
    }
    _initialized = false;
}


bool UARTManager::poll(int ms)
{
    // TODO check serial port command link and input data
    fnSystem.delay_microseconds(500); // TODO use ms parameter
    return false;
}

#if defined(_WIN32)
// TODO
// only stubs here
void UARTManager::set_port(const char *device, int command_pin, int proceed_pin) 
{
    if (device != nullptr)
        strlcpy(_device, device, sizeof(_device));
    else
        _device[0] = 0;
    _command_pin = command_pin;
    _proceed_pin = proceed_pin;
};
const char* UARTManager::get_port(int &command_pin, int &proceed_pin)
{
    command_pin = _command_pin;
    proceed_pin = _proceed_pin;
    return _device;
}
void UARTManager::begin(int baud)
{
    if(_initialized)
    {
        end();
    }
    Debug_printf("### UART initialized ###\n");
    // Set initialized.
    _initialized=true;
    set_baudrate(baud);
}
void UARTManager::suspend(int sec)
{
    Debug_println("Suspending serial port");
}
void UARTManager::flush_input()
{
}

/* Clears input buffer and flushes out transmit buffer waiting at most
   waiting MAX_FLUSH_WAIT_TICKS until all sends are completed
*/
void UARTManager::flush()
{
}

/* Returns number of bytes available in receive buffer or -1 on error
*/
int UARTManager::available()
{
    return 0;
}
void UARTManager::set_baudrate(uint32_t baud)
{
    Debug_printf("UART set_baudrate: %d\n", baud);
    _baud = baud;
}
bool UARTManager::command_asserted(void)
{
    return false;
}
void UARTManager::set_proceed(bool level)
{
}
timeval timeval_from_ms(const uint32_t millis)
{
  timeval tv;
  tv.tv_sec = millis / 1000;
  tv.tv_usec = (millis - (tv.tv_sec * 1000)) * 1000;
  return tv;
}
bool UARTManager::waitReadable(uint32_t timeout_ms)
{
    return false;
}
int UARTManager::read(void)
{
    uint8_t byte;
    return (readBytes(&byte, 1) == 1) ? byte : -1;
}
size_t UARTManager::readBytes(uint8_t *buffer, size_t length, bool command_mode)
{
    return 0;
}
size_t UARTManager::write(uint8_t c)
{
    return write(&c, 1);
}

size_t UARTManager::write(const uint8_t *buffer, size_t size)
{
    return 0;
}

#else

void UARTManager::set_port(const char *device, int command_pin, int proceed_pin)
{
    if (device != nullptr)
        strlcpy(_device, device, sizeof(_device));
    else
        _device[0] = 0;
    _command_pin = command_pin;
    _proceed_pin = proceed_pin;
    switch (_command_pin)
    {
// TODO move from fnConfig.h to fnUART.h
// enum serial_command_pin
// {
//     SERIAL_COMMAND_NONE = 0,
//     SERIAL_COMMAND_DSR,
//     SERIAL_COMMAND_CTS,
//     SERIAL_COMMAND_RI,
//     SERIAL_COMMAND_INVALID
// };
    case 1: // SERIAL_COMMAND_DSR
        _command_tiocm = TIOCM_DSR;
        break;
    case 2: // SERIAL_COMMAND_CTS
        _command_tiocm = TIOCM_CTS;
        break;
    case 3: // SERIAL_COMMAND_RI
        _command_tiocm = TIOCM_RI;
        break;
    default:
        _command_tiocm = 0;
    }

// TODO move from fnConfig.h to fnUART.h
// enum serial_proceed_pin
// {
//     SERIAL_PROCEED_NONE = 0,
//     SERIAL_PROCEED_DTR,
//     SERIAL_PROCEED_RTS,
//     SERIAL_PROCEED_INVALID
// };
    switch (_proceed_pin)
    {
    case 1: // SERIAL_PROCEED_DTR
        _proceed_tiocm = TIOCM_DTR;
        break;
    case 2: // SERIAL_PROCEED_RTS
        _proceed_tiocm = TIOCM_RTS;
        break;
    }
}

const char* UARTManager::get_port(int &command_pin, int &proceed_pin)
{
    command_pin = _command_pin;
    proceed_pin = _proceed_pin;
    return _device;
}

void UARTManager::begin(int baud)
{
    if(_initialized)
    {
        end();
    }

    _errcount = 0;
    _suspend_time = 0;

    // Open the serial port
    if (*_device == 0)
    {
        // Probe some serial ports
        Debug_println("Trying " UART_PROBE_DEV1);
        if ((_fd = open(UART_PROBE_DEV1, O_RDWR | O_NOCTTY | O_NONBLOCK)) >= 0)
            strlcpy(_device, UART_PROBE_DEV1, sizeof(_device));
        else
        {
            Debug_println("Trying " UART_PROBE_DEV2);
            if ((_fd = open(UART_PROBE_DEV2, O_RDWR | O_NOCTTY | O_NONBLOCK)) >= 0)
                strlcpy(_device, UART_PROBE_DEV2, sizeof(_device));
        }

        // successfull probe
        if (*_device != 0)
            Debug_printf("Setting up serial port %s\n", _device);
    }
    else
    {
        Debug_printf("Setting up serial port %s\n", _device);
        _fd = open(_device, O_RDWR | O_NOCTTY | O_NONBLOCK);
    }

    if (_fd < 0)
    {
        Debug_printf("Failed to open serial port, error %d: %s\n", errno, strerror(errno));
        suspend();
		return;
	}

#if defined(__linux__)
    // Enable low latency
	struct serial_struct ss;
    if (-1 == ioctl(_fd, TIOCGSERIAL, &ss))
    {
        Debug_printf("TIOCGSERIAL error %d: %s\n", errno, strerror(errno));
        suspend();
        return;
    }
	ss.flags |= ASYNC_LOW_LATENCY;
    if (-1 == ioctl(_fd, TIOCSSERIAL, &ss))
        Debug_printf("TIOCSSERIAL error %d: %s\n", errno, strerror(errno));
#endif

#ifdef BUILD_ADAM
    if (_uart_num == 2)
        uart_set_line_inverse(_uart_num, UART_SIGNAL_TXD_INV | UART_SIGNAL_RXD_INV);
#endif /* BUILD_ADAM */

    // Read in existing settings
    struct termios tios;
    if(tcgetattr(_fd, &tios) != 0)
    {
        Debug_printf("tcgetattr error %d: %s\n", errno, strerror(errno));
        suspend();
		return;
    }

    tios.c_cflag &= ~PARENB; // Clear parity bit, disabling parity (most common)
    tios.c_cflag &= ~CSTOPB; // Clear stop field, only one stop bit used in communication (most common)
    tios.c_cflag &= ~CSIZE; // Clear all bits that set the data size 
    tios.c_cflag |= CS8; // 8 bits per byte (most common)
    tios.c_cflag &= ~CRTSCTS; // Disable RTS/CTS hardware flow control (most common)
    tios.c_cflag |= CREAD | CLOCAL; // Turn on READ & ignore ctrl lines (CLOCAL = 1)
  
    tios.c_lflag &= ~ICANON;
    tios.c_lflag &= ~ECHO; // Disable echo
    tios.c_lflag &= ~ECHOE; // Disable erasure
    tios.c_lflag &= ~ECHONL; // Disable new-line echo
    tios.c_lflag &= ~ISIG; // Disable interpretation of INTR, QUIT and SUSP
    tios.c_iflag &= ~(IXON | IXOFF | IXANY); // Turn off s/w flow ctrl
    tios.c_iflag &= ~(IGNBRK|BRKINT|PARMRK|ISTRIP|INLCR|IGNCR|ICRNL); // Disable any special handling of received bytes
  
    tios.c_oflag &= ~OPOST; // Prevent special interpretation of output bytes (e.g. newline chars)
    tios.c_oflag &= ~ONLCR; // Prevent conversion of newline to carriage return/line feed
    // tios.c_oflag &= ~OXTABS; // Prevent conversion of tabs to spaces (NOT PRESENT ON LINUX)
    // tios.c_oflag &= ~ONOEOT; // Prevent removal of C-d chars (0x004) in output (NOT PRESENT ON LINUX)

    tios.c_cc[VTIME] = 0;
    tios.c_cc[VMIN] = 0;

    // Apply settings
    if (tcsetattr(_fd, TCSANOW, &tios) != 0)
    {
        Debug_printf("tcsetattr error %d: %s\n", errno, strerror(errno));
        suspend();
		return;
    }


    Debug_printf("### UART initialized ###\n");
    // Set initialized.
    _initialized = true;
    set_baudrate(baud);
}


void UARTManager::suspend(int sec)
{
    Debug_println("Suspending serial port");
    struct timeval tv;
    gettimeofday(&tv, NULL);
    _suspend_time = tv.tv_sec + sec;
    end();
}

/* Discards anything in the input buffer
 */
void UARTManager::flush_input()
{
    if (_initialized)
        tcflush(_fd, TCIFLUSH);
}

/* Clears input buffer and flushes out transmit buffer waiting at most
   waiting MAX_FLUSH_WAIT_TICKS until all sends are completed
*/
void UARTManager::flush()
{
    if (_initialized)
        tcdrain(_fd);
}

/* Returns number of bytes available in receive buffer or -1 on error
 */
int UARTManager::available()
{
    int result;
    if (!_initialized)
        return 0;
	if (ioctl(_fd, FIONREAD, &result) < 0)
        return 0;
    return result;
}

/* Changes baud rate
*/
void UARTManager::set_baudrate(uint32_t baud)
{
    Debug_printf("UART set_baudrate: %d\n", baud);

    if (!_initialized)
        return;

    if (baud == 0)
        baud = 19200;

    int baud_id = B0;  // B0 to indicate custom speed

    switch (baud)
    {
        case 300:
            baud_id = B300;
            break;
        case 600:
            baud_id = B600;
            break;
        case 1200:
            baud_id = B1200;
            break;
        case 1800:
            baud_id = B1800;
            break;
        case 2400:
            baud_id = B2400;
            break;
        case 4800:
            baud_id = B4800;
            break;
        case 9600:
            baud_id = B9600;
            break;
        case 19200:
            baud_id = B19200;
            break;
        case 38400:
            baud_id = B38400;
            break;
        case 57600:
            baud_id = B57600;
            break;
        case 115200:
            baud_id = B115200;
            break;
    }

    if (baud_id == B0)
    {
        // custom baud rate
#if defined(__APPLE__) && defined (IOSSIOSPEED)
        // OS X support

        speed_t new_baud = (speed_t) baud;
        if (-1 == ioctl(_fd, IOSSIOSPEED, &new_baud, 1))
            Debug_printf("IOSSIOSPEED error %d: %s\n", errno, strerror(errno));

#elif defined(__linux__)
        // Linux Support

		struct termios2 tios2;
		
		if (-1 == ioctl(_fd, TCGETS2, &tios2))
        {
            Debug_printf("TCGETS2 error %d: %s\n", errno, strerror(errno));
            return;
		}
		tios2.c_cflag &= ~(CBAUD | CBAUD << LINUX_IBSHIFT);
		tios2.c_cflag |= BOTHER | BOTHER << LINUX_IBSHIFT;
		tios2.c_ispeed = baud;
		tios2.c_ospeed = baud;
		if (-1 == ioctl(_fd, TCSETS2, &tios2))
            Debug_printf("TCSETS2 error %d: %s\n", errno, strerror(errno));
#else
        Debug_println("Custom baud rate is not implemented");
#endif
    }
    else
    {
        // standard speeds
        termios tios;
        if(tcgetattr(_fd, &tios) != 0)
            Debug_printf("tcgetattr error %d: %s\n", errno, strerror(errno));
        cfsetspeed(&tios, baud_id);
        // Apply settings
        if (tcsetattr(_fd, TCSANOW, &tios) != 0)
            Debug_printf("tcsetattr error %d: %s\n", errno, strerror(errno));
    }
    _baud = baud;
}

bool UARTManager::command_asserted(void)
{
    int status;

    if (! _initialized)
    {
        // is serial port suspended ?
        if (_suspend_time != 0)
        {
            struct timeval tv;
            gettimeofday(&tv, NULL);
            if (_suspend_time > tv.tv_sec)
                return false;
            // try to re-open serial port
            begin(_baud);
        }
        if (! _initialized)
            return false;
    }

    if (ioctl(_fd, TIOCMGET, &status) < 0)
    {
        // handle serial port errors
        _errcount++;
        if(_errcount == 1)
            Debug_printf("UART command_asserted() TIOCMGET error %d: %s\n", errno, strerror(errno));
        else if (_errcount > 1000)
            suspend();
        return false;
    }
    _errcount = 0;

    return ((status & _command_tiocm) != 0);
}

void UARTManager::set_proceed(bool level)
{
    static int last_level = -1; // 0,1 or -1 for unknown
    int new_level = level ? 0 : 1;
    int result;

    if (!_initialized)
        return;
    if (last_level == new_level)
        return;

    Debug_print(level ? "+" : "-");
    last_level = new_level;

    unsigned long request = level ? TIOCMBIC : TIOCMBIS;
    result = ioctl(_fd, request, &_proceed_tiocm);
    if (result < 0)
        Debug_printf("UART set_proceed() ioctl error %d: %s\n", errno, strerror(errno));
}

timeval timeval_from_ms(const uint32_t millis)
{
  timeval tv;
  tv.tv_sec = millis / 1000;
  tv.tv_usec = (millis - (tv.tv_sec * 1000)) * 1000;
  return tv;
}

bool UARTManager::waitReadable(uint32_t timeout_ms)
{
    // Setup a select call to block for serial data or a timeout
    fd_set readfds;
    FD_ZERO(&readfds);
    FD_SET(_fd, &readfds);
    timeval timeout_tv(timeval_from_ms(timeout_ms));

    int result = select(_fd + 1, &readfds, nullptr, nullptr, &timeout_tv);

    if (result < 0) 
    {
        if (errno != EINTR)
        {
            Debug_printf("UART waitReadable() select error %d: %s\n", errno, strerror(errno));
        }
        return false;
    }
    // Timeout occurred
    if (result == 0)
    {
        return false;
    }
    // This shouldn't happen, if result > 0 our fd has to be in the list!
    if (!FD_ISSET (_fd, &readfds)) 
    {
        Debug_println("UART waitReadable() unexpected select result");
    }
    // Data available to read.
    return true;
}

/* Returns a single byte from the incoming stream
 */
int UARTManager::read(void)
{
    uint8_t byte;
    return (readBytes(&byte, 1) == 1) ? byte : -1;
}

/* Since the underlying Stream calls this Read() multiple times to get more than one
 *  character for ReadBytes(), we override with a single call to uart_read_bytes
 */
size_t UARTManager::readBytes(uint8_t *buffer, size_t length, bool command_mode)
{
    if (!_initialized)
        return 0;

    int result;
    int rxbytes;
    for (rxbytes=0; rxbytes<length;)
    {
        result = ::read(_fd, &buffer[rxbytes], length-rxbytes);
        // Debug_printf("read: %d\n", result);
        if (result < 0)
        {
            if (errno == EAGAIN)
            {
                result = 0;
            }
            else
            {
                Debug_printf("UART readBytes() read error %d: %s\n", errno, strerror(errno));
                break;
            }
        }

        rxbytes += result;
        if (rxbytes == length)
        {
            // done
            break;
        }

        // // wait for more data
        // if (command_mode && !command_asserted())
        // {
        //     Debug_println("### UART readBytes() CMD pin deasserted while reading command ###");
        //     return 1 + length; // indicate to SIO caller
        // }
        if (!waitReadable(500)) // 500 ms timeout
        {
            Debug_println("UART readBytes() TIMEOUT");
            break;
        }
    }
    return rxbytes;
}

size_t UARTManager::write(uint8_t c)
{
    // int z = uart_write_bytes(_uart_num, (const char *)&c, 1);
    // //uart_wait_tx_done(_uart_num, MAX_WRITE_BYTE_TICKS);
    // return z;
    return write(&c, 1);
}

size_t UARTManager::write(const uint8_t *buffer, size_t size)
{
    if (!_initialized)
        return 0;

    int result;
    int txbytes;
    fd_set writefds;
    timeval timeout_tv(timeval_from_ms(500));

    for (txbytes=0; txbytes<size;)
    {
        FD_ZERO(&writefds);
        FD_SET(_fd, &writefds);

        // Debug_printf("select(%lu)\n", timeout_tv.tv_sec*1000+timeout_tv.tv_usec/1000);
        int result = select(_fd + 1, NULL, &writefds, NULL, &timeout_tv);

        if (result < 0) 
        {
            // Select was interrupted, try again
            if (errno == EINTR) 
            {
                continue;
            }
            // Otherwise there was some error
            Debug_printf("UART write() select error %d: %s\n", errno, strerror(errno));
            break;
        }

        // Timeout
        if (result == 0)
        {
            Debug_println("UART write() TIMEOUT");
            break;
        }

        if (result > 0) 
        {
            // Make sure our file descriptor is in the ready to write list
            if (FD_ISSET(_fd, &writefds))
            {
                // This will write some
                result = ::write(_fd, &buffer[txbytes], size-txbytes);
                // Debug_printf("write: %d\n", result);
                if (result < 1)
                {
                    Debug_printf("UART write() write error %d: %s\n", errno, strerror(errno));
                    break;
                }

                txbytes += result;
                if (txbytes == size)
                {
                    // TODO is flush missing somewhere else?
                    // wait UART is writable again before return
                    timeout_tv = timeval_from_ms(1000 + result * 12500 / _baud);
                    select(_fd + 1, NULL, &writefds, NULL, &timeout_tv);
                    // done
                    break;
                }
                if (txbytes < size)
                {
                    timeout_tv = timeval_from_ms(1000 + result * 12500 / _baud);
                    continue;
                }
            }
            // This shouldn't happen, if r > 0 our fd has to be in the list!
            Debug_println("UART write() unexpected select result");
        }
    }
    return txbytes;
}

#endif // _WIN32

size_t UARTManager::write(const char *str)
{
    // int z = uart_write_bytes(_uart_num, str, strlen(str));
    // return z;
    return write((const uint8_t *)str, strlen(str));
}

/*
size_t UARTManager::printf(const char * fmt...)
{
    char *result = nullptr;
    va_list vargs;

    if (!_initialized)
        return -1;

    va_start(vargs, fmt);

    int z = vasprintf(&result, fmt, vargs);

    if (z > 0)
        uart_write_bytes(_uart_num, result, z);

    va_end(vargs);

    if (result != nullptr)
        free(result);

    // uart_wait_tx_done(_uart_num, MAX_WRITE_BUFFER_TICKS);

    return z >= 0 ? z : 0;
}
*/

size_t UARTManager::_print_number(unsigned long n, uint8_t base)
{
    char buf[8 * sizeof(long) + 1]; // Assumes 8-bit chars plus zero byte.
    char *str = &buf[sizeof(buf) - 1];

    if (!_initialized)
        return 0;

    *str = '\0';

    // prevent crash if called with base == 1
    if (base < 2)
        base = 10;

    do
    {
        unsigned long m = n;
        n /= base;
        char c = m - base * n;
        *--str = c < 10 ? c + '0' : c + 'A' - 10;
    } while (n);

    return write(str);
}

size_t UARTManager::print(const char *str)
{
//     int z = strlen(str);

    if (!_initialized)
        return 0;

//     return uart_write_bytes(_uart_num, str, z);;
    return write(str);
}

size_t UARTManager::print(std::string str)
{
    if (!_initialized)
        return 0;

    return print(str.c_str());
}

size_t UARTManager::print(int n, int base)
{
    if (!_initialized)
        return 0;

    return print((long)n, base);
}

size_t UARTManager::print(unsigned int n, int base)
{
    if (!_initialized)
        return 0;

    return print((unsigned long)n, base);
}

size_t UARTManager::print(long n, int base)
{
    if (!_initialized)
        return 0;

    if (base == 0)
    {
        return write(n);
    }
    else if (base == 10)
    {
        if (n < 0)
        {
            // int t = print('-');
            int t = print("-");
            n = -n;
            return _print_number(n, 10) + t;
        }
        return _print_number(n, 10);
    }
    else
    {
        return _print_number(n, base);
    }
}

size_t UARTManager::print(unsigned long n, int base)
{
    if (!_initialized)
        return 0;

    if (base == 0)
    {
        return write(n);
    }
    else
    {
        return _print_number(n, base);
    }
}

/*
size_t UARTManager::println(const char *str)
{
    if (!_initialized)
        return -1;

    size_t n = print(str);
    n += println();
    return n;
}

size_t UARTManager::println(std::string str)
{
    if (!_initialized)
        return -1;

    size_t n = print(str);
    n += println();
    return n;
}

size_t UARTManager::println(int num, int base)
{
    if (!_initialized)
        return -1;

    size_t n = print(num, base);
    n += println();
    return n;
}
*/


/*

size_t Print::print(const __FlashStringHelper *ifsh)
{
    return print(reinterpret_cast<const char *>(ifsh));
}


size_t Print::print(char c)
{
    return write(c);
}

size_t Print::print(unsigned char b, int base)
{
    return print((unsigned long) b, base);
}


size_t Print::print(double n, int digits)
{
    return printFloat(n, digits);
}

size_t Print::println(const __FlashStringHelper *ifsh)
{
    size_t n = print(ifsh);
    n += println();
    return n;
}

size_t Print::print(const Printable& x)
{
    return x.printTo(*this);
}

size_t Print::print(struct tm * timeinfo, const char * format)
{
    const char * f = format;
    if(!f){
        f = "%c";
    }
    char buf[64];
    size_t written = strftime(buf, 64, f, timeinfo);
    if(written == 0){
        return written;
    }
    return print(buf);
}

size_t Print::println(char c)
{
    size_t n = print(c);
    n += println();
    return n;
}

size_t Print::println(unsigned char b, int base)
{
    size_t n = print(b, base);
    n += println();
    return n;
}

size_t Print::println(unsigned int num, int base)
{
    size_t n = print(num, base);
    n += println();
    return n;
}

size_t Print::println(long num, int base)
{
    size_t n = print(num, base);
    n += println();
    return n;
}

size_t Print::println(unsigned long num, int base)
{
    size_t n = print(num, base);
    n += println();
    return n;
}

size_t Print::println(double num, int digits)
{
    size_t n = print(num, digits);
    n += println();
    return n;
}

size_t Print::println(const Printable& x)
{
    size_t n = print(x);
    n += println();
    return n;
}

size_t Print::println(struct tm * timeinfo, const char * format)
{
    size_t n = print(timeinfo, format);
    n += println();
    return n;
}
*/
