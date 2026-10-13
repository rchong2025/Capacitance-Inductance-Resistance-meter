import com.fazecast.jSerialComm.SerialPort;
import javax.swing.SwingUtilities;
import java.awt.*;
import java.awt.print.*;
import java.io.BufferedReader;
import java.io.IOException;
import java.io.InputStreamReader;
import java.lang.reflect.InvocationTargetException;
import java.util.Collections;
import java.util.List;
import java.util.ArrayList;
import java.util.Scanner;
import java.io.InputStream;
import javax.print.PrintService;
import javax.print.PrintServiceLookup;
import java.awt.print.PrinterException;
import java.awt.print.PrinterJob;
import java.awt.print.PageFormat;
import java.awt.print.Printable;


public class Main implements Printable {

    private static final List<String> cap = Collections.synchronizedList(new ArrayList<>());

    public static void main(String[] args) throws IOException {
        Main app = new Main();
        app.startSerialListener();
    }

    public void startSerialListener() throws IOException {
        SerialPort comPort;
        SerialPort[] availablePorts = SerialPort.getCommPorts();
        if (availablePorts.length == 0) {
            System.out.println("Error: No serial ports found.");
            return;
        }

        System.out.println("Available ports:");
        for (int i = 0; i < availablePorts.length; i++) {
            System.out.println("  [" + i + "] " + availablePorts[i].getSystemPortName());
        }

        comPort = availablePorts[0];
        System.out.println("Using port: " + comPort.getSystemPortName());

        comPort.setBaudRate(115200);
        comPort.setNumDataBits(8);
        comPort.setNumStopBits(SerialPort.ONE_STOP_BIT);
        comPort.setParity(SerialPort.NO_PARITY);
        comPort.setComPortTimeouts(SerialPort.TIMEOUT_READ_BLOCKING, 0, 0);

        if (!comPort.openPort()) {
            System.out.println("Error: Could not open " + comPort.getSystemPortName());
            return;
        }

        System.out.println("Port opened. Waiting for data...");

        InputStream in = comPort.getInputStream();
        StringBuilder sb = new StringBuilder();
        boolean keepRunning = true;

        try {
            int b;
            while (keepRunning && (b = in.read()) != -1) {
                if (b == '\n' || b == '\r') {
                    String line = sb.toString().trim();
                    sb.setLength(0);
                    if (line.length() < 4) continue;

                    char type = line.charAt(line.length() - 1);
                    char addFlag = line.charAt(line.length() - 3);
                    char printFlag = line.charAt(line.length() - 2);
                    String value = line.substring(0, line.length() - 3);

                    if (addFlag == 'Y') {
                        cap.add(value);
                        System.out.println("Added: " + value);
                    }
                    if (printFlag == 'Y') {
                        System.out.println("Printing...");
                        triggerPrintJob();
                        keepRunning = false;
                    }
                } else {
                    sb.append((char) b);
                }
            }
        } catch (Exception e) {
            System.out.println("Error: " + e.getMessage());
        } finally {
            in.close();
            comPort.closePort();
            System.out.println("Port closed.");
        }
    }

    private void triggerPrintJob() {
        try {
            // Hand off to the EDT and wait for it to finish
            SwingUtilities.invokeAndWait(() -> {
                PrinterJob job = PrinterJob.getPrinterJob();
                job.setPrintable(this);

                if (job.printDialog()) {
                    try {
                        job.print();
                    } catch (PrinterException e) {
                        System.err.println("Printing failed: " + e.getMessage());
                    }
                }
            });
        } catch (InterruptedException e) {
            Thread.currentThread().interrupt(); // Restore interrupt flag
            System.err.println("Print interrupted: " + e.getMessage());
        } catch (InvocationTargetException e) {
            System.err.println("Print error: " + e.getCause().getMessage());
        }
    }

    @Override
    public int print(Graphics g, PageFormat pf, int pageIndex) throws PrinterException {
        Graphics2D g2 = (Graphics2D) g;
        g2.translate(pf.getImageableX(), pf.getImageableY());

        double pageHeight = pf.getImageableHeight();
        int lineHeight    = 15;
        int headerOffset  = 80; // Y position where entries start
        int linesPerPage  = (int) ((pageHeight - headerOffset) / lineHeight);

        // Calculate which entries belong on this page
        int startIndex = pageIndex * linesPerPage;
        if (startIndex >= cap.size() && pageIndex > 0) {
            return NO_SUCH_PAGE; // No more pages needed
        }

        // Draw header on every page
        g2.setFont(new Font("Monospaced", Font.BOLD, 14));
        g2.drawString("CAPACITOR/INDUCTOR/RESISTANCE VALUES", 50, 50);
        g2.drawLine(50, 55, 300, 55);

        // Draw page number
        g2.setFont(new Font("Monospaced", Font.ITALIC, 10));
        g2.drawString("Page " + (pageIndex + 1), 50, (int) pageHeight - 10);

        // Draw the entries for this page
        g2.setFont(new Font("Monospaced", Font.PLAIN, 12));
        int yOffset  = headerOffset;
        int endIndex = Math.min(startIndex + linesPerPage, cap.size());

        for (int i = startIndex; i < endIndex; i++) {
            g2.drawString("- " + cap.get(i), 50, yOffset);
            yOffset += lineHeight;
        }

        return PAGE_EXISTS;
    }
}