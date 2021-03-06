// license:BSD-3-Clause
// copyright-holders:smf
#include "terminal.h"

serial_terminal_device::serial_terminal_device(const machine_config &mconfig, const char *tag, device_t *owner, UINT32 clock)
	: generic_terminal_device(mconfig, SERIAL_TERMINAL, "Serial Terminal", tag, owner, clock, "serial_terminal", __FILE__)
	, device_serial_interface(mconfig, *this)
	, device_rs232_port_interface(mconfig, *this)
	, m_rs232_txbaud(*this, "RS232_TXBAUD")
	, m_rs232_rxbaud(*this, "RS232_RXBAUD")
	, m_rs232_startbits(*this, "RS232_STARTBITS")
	, m_rs232_databits(*this, "RS232_DATABITS")
	, m_rs232_parity(*this, "RS232_PARITY")
	, m_rs232_stopbits(*this, "RS232_STOPBITS")
	, m_fifo{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }
	, m_head(0)
	, m_tail(0)
	, m_empty(0)
{
}

static INPUT_PORTS_START(serial_terminal)
	PORT_INCLUDE(generic_terminal)

	MCFG_RS232_BAUD("RS232_TXBAUD", RS232_BAUD_9600, "TX Baud", serial_terminal_device, update_serial)
	MCFG_RS232_BAUD("RS232_RXBAUD", RS232_BAUD_9600, "RX Baud", serial_terminal_device, update_serial)
	MCFG_RS232_STARTBITS("RS232_STARTBITS", RS232_STARTBITS_1, "Start Bits", serial_terminal_device, update_serial)
	MCFG_RS232_DATABITS("RS232_DATABITS", RS232_DATABITS_8, "Data Bits", serial_terminal_device, update_serial)
	MCFG_RS232_PARITY("RS232_PARITY", RS232_PARITY_NONE, "Parity", serial_terminal_device, update_serial)
	MCFG_RS232_STOPBITS("RS232_STOPBITS", RS232_STOPBITS_1, "Stop Bits", serial_terminal_device, update_serial)
INPUT_PORTS_END

ioport_constructor serial_terminal_device::device_input_ports() const
{
	return INPUT_PORTS_NAME(serial_terminal);
}

void serial_terminal_device::device_start()
{
	generic_terminal_device::device_start();
	device_serial_interface::register_save_state(machine().save(), this);

	save_item(NAME(m_fifo));
	save_item(NAME(m_head));
	save_item(NAME(m_tail));
	save_item(NAME(m_empty));
}

WRITE_LINE_MEMBER(serial_terminal_device::update_serial)
{
	std::fill(std::begin(m_fifo), std::end(m_fifo), 0U);
	m_head = m_tail = 0;
	m_empty = 1;

	int const startbits = convert_startbits(m_rs232_startbits->read());
	int const databits = convert_databits(m_rs232_databits->read());
	parity_t const parity = convert_parity(m_rs232_parity->read());
	stop_bits_t const stopbits = convert_stopbits(m_rs232_stopbits->read());

	set_data_frame(startbits, databits, parity, stopbits);

	int const txbaud = convert_baud(m_rs232_txbaud->read());
	set_tra_rate(txbaud);

	int const rxbaud = convert_baud(m_rs232_rxbaud->read());
	set_rcv_rate(rxbaud);

	output_rxd(1);

	// TODO: make this configurable
	output_dcd(0);
	output_dsr(0);
	output_cts(0);
	receive_register_reset();
	transmit_register_reset();
}

void serial_terminal_device::device_reset()
{
	generic_terminal_device::device_reset();

	update_serial(0);
}

void serial_terminal_device::device_timer(emu_timer &timer, device_timer_id id, int param, void *ptr)
{
	generic_terminal_device::device_timer(timer, id, param, ptr);
	device_serial_interface::device_timer(timer, id, param, ptr);
}

void serial_terminal_device::send_key(UINT8 code)
{
	assert(!m_empty || (m_head == m_tail));
	assert(m_head < ARRAY_LENGTH(m_fifo));
	assert(m_tail < ARRAY_LENGTH(m_fifo));

	if (m_empty && is_transmit_register_empty())
	{
		transmit_register_setup(code);
	}
	else if (m_empty || (m_head != m_tail))
	{
		m_fifo[m_tail] = code;
		m_tail = (m_tail + 1) & 0x0fU;
		m_empty = 0;
	}
	else
	{
		logerror("FIFO overrun (code = 0x%02x)", code);
	}
}

void serial_terminal_device::tra_callback()
{
	output_rxd(transmit_register_get_data_bit());
}

void serial_terminal_device::tra_complete()
{
	assert(!m_empty || (m_head == m_tail));
	assert(m_head < ARRAY_LENGTH(m_fifo));
	assert(m_tail < ARRAY_LENGTH(m_fifo));

	if (!m_empty)
	{
		transmit_register_setup(m_fifo[m_head]);
		m_head = (m_head + 1) & 0x0fU;
		m_empty = (m_head == m_tail) ? 1 : 0;
	}
}

void serial_terminal_device::rcv_complete()
{
	receive_register_extract();
	term_write(get_received_char());
}

const device_type SERIAL_TERMINAL = &device_creator<serial_terminal_device>;
