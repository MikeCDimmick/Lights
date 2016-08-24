from Tkinter import *
from bluetooth import *
from tkColorChooser import askcolor
import pickle
import sys
import time
import random

class Example(Frame):
  
    def __init__(self, parent):
        Frame.__init__(self, parent)   
        self.parent = parent
        self.initUI()

    #main UI
    def initUI(self):

        #setup
        self.parent.title("Lights")
        frame = Frame(self, relief=RAISED, borderwidth=1)
        frame.pack(fill=BOTH, expand=True)
        self.pack(fill=BOTH, expand=True)
        self.pack(fill=BOTH, expand=1)

        #background image
        self.backgroundImage = PhotoImage(file="grad.gif")
        self.backgroundLabel = Label(self,image=self.backgroundImage)
        self.backgroundLabel.place(x=0, y=0, relwidth=1.0, relheight=1.0, anchor="nw")


        self.setEntireStrip = Button(self, text="Random", command = self.randomColour,bg='black', fg = "white",)
        self.setEntireStrip.pack(side="right")
        self.setEntireStrip.place(relx=.25, rely=.9, anchor="c")

        #button change colour
        self.setEntireStrip = Button(self, text="Custom", command = self.getColor,bg='black', fg = "white",)
        self.setEntireStrip.pack(side="right")
        self.setEntireStrip.place(relx=.25, rely=.8, anchor="c")

        #button Choose pattern
        self.setEntireStrip = Button(self, text="Patterns", command = self.getColor,bg='black', fg = "white",)
        self.setEntireStrip.pack(side="right")
        self.setEntireStrip.place(relx=.25, rely=.7, anchor="c")

        #button Turn off lights
        self.setEntireStrip = Button(self, text="Off", command = self.Off,bg='black', fg = "white", height = 10, width = 40)
        self.setEntireStrip.pack(side="right")
        self.setEntireStrip.place(relx=.75, rely=.25, anchor="c")

        #button brighten lights
        self.setEntireStrip = Button(self, text="up", command = self.On,bg='black', fg = "white",height = 5, width = 40)
        self.setEntireStrip.pack(side="right")
        self.setEntireStrip.place(relx=.25, rely=.195, anchor="c")

        #button dim lights
        self.setEntireStrip = Button(self, text="down", command = self.On,bg='black', fg = "white",height = 5, width = 40)
        self.setEntireStrip.pack(side="right")
        self.setEntireStrip.place(relx=.25, rely=.31, anchor="c")

        #button choose preset colour
        self.setEntireStrip = Button(self, text="Smart Lights", command = self.smartLights,bg='black', fg = "white",)
        self.setEntireStrip.pack(side="right")
        self.setEntireStrip.place(relx=.25, rely=.5, anchor="c")

        #button connect to bluetooth
        self.bluetoothButton = Button(self, text="Connect to a bluetooth device", command=self.bluetooth_window,bg='black', fg = "white", activebackground = 'black', activeforeground= 'black')
        self.bluetoothButton.place(relx=.5, rely=.05, anchor="c")

        #button use layout manager
        #self.layoutManagerButton = Button(self, text="Layout Manager", command=self.layoutManager_window,bg='black', fg = "white", activebackground = 'black', activeforeground= 'black')
        #self.layoutManagerButton.place(relx=.5, rely=.3, anchor="c")

    #window where you select which bluetooth device to connect to
    def bluetooth_window(self):

        #setup window, label and buttons
        self.bluetooth_window = Toplevel(self)
        self.bluetooth_window.resizable(0,0)
        self.pack(fill=BOTH, expand=1)

        self.bluetooth_window.wm_title("Searching...")
        self.bluetooth_window.wm_iconbitmap('bluetooth_icon.ico')

        self.bluetooth_window.connectButton = Button(self.bluetooth_window, text="Connect", pady = 10, padx = 10, command=self.connectBluetoothDevice)
        self.bluetooth_window.connectButton.pack(side="top")

        #search for devices
        self.bluetooth_window.devices = []
        self.bluetooth_window.sock = []
        self.bluetooth_window.chosenDeviceName = ""
        self.bluetooth_window.chosenDeviceAddress = ""
        device_addresses = discover_devices()

        for address in device_addresses:
            self.bluetooth_window.devices.append([str(lookup_name(address)), str(address)])

        #update user on results
        self.bluetooth_window.wm_title("Bluetooth devices found.")

        #create list of devices to connect to
        self.bluetooth_window.bluetoothList = Listbox(self.bluetooth_window,width = 40)
        self.bluetooth_window.bluetoothList.bind("<<ListboxSelect>>", self.onBluetoothSelect)    
        for i in self.bluetooth_window.devices:
            self.bluetooth_window.bluetoothList.insert(END, i[0])
            
        self.bluetooth_window.bluetoothList.pack(pady=10)

        #bring to their attention
        raise_above_all(self.bluetooth_window)

    def layoutManager_window(self):

        #setup
        self.layoutManager_window = Toplevel(self)
        self.pack(fill=BOTH, expand=1)
        self.layoutManager_window.resizable(0,0)

        self.layoutManager_window.wm_title("Layout Manager")
        self.layoutManager_window.wm_iconbitmap('lights_icon.ico')

        self.layoutManager_window.layoutManagerInfo = Label(self.layoutManager_window, text="Create strips in the order they appear in the physical wiring")
        self.layoutManager_window.layoutManagerInfo.pack(side="top", fill="both", expand=True, padx=10, pady=10)

        #display list of strips
        self.layoutManager_window.strips = []
        self.layoutManager_window.stripList = Listbox(self.layoutManager_window ,width = 40)
        self.layoutManager_window.stripList.bind("<<ListboxSelect>>", self.onStripSelect)    

        for i in self.layoutManager_window.strips:
            self.layoutManager_window.stripList.insert(END, i)
            
        self.layoutManager_window.stripList.pack(pady=5)

        #button to create new strip
        self.layoutManager_window.detailButton = Button(self.layoutManager_window, text="New Strip...", pady = 10, padx = 10, command=self.setStripDetails_window)
        self.layoutManager_window.detailButton.pack(side="right")

        #button to edit selection
        self.layoutManager_window.stripToEdit = ""
        self.layoutManager_window.createButton = Button(self.layoutManager_window, text="Edit Strip", pady = 10, padx = 10, command=self.editStripDetails_window)
        self.layoutManager_window.createButton.pack(side="right")

        #button to remove strip
        self.layoutManager_window.stripToRemove = ""
        self.layoutManager_window.removeButton = Button(self.layoutManager_window, text="Remove Strip", pady = 10, padx = 10, command=self.removeStrip)
        self.layoutManager_window.removeButton.pack(side="right")

        #button to save layout
        self.layoutManager_window.saveButton = Button(self.layoutManager_window, text="Save Layout", pady = 10, padx = 10, command=self.saveLayout)
        self.layoutManager_window.saveButton.pack(side="right")

        #bring to their attention
        raise_above_all(self.layoutManager_window)

    def setStripDetails_window(self):

        #setup screen, labels, and text boxes
        self.setStripDetails_window = Toplevel(self)
        self.setStripDetails_window.resizable(0,0)
        self.pack(fill=BOTH, expand=1)
        self.setStripDetails_window.wm_title("Strip Details")
        self.setStripDetails_window.wm_iconbitmap('lights_icon.ico')

        self.setStripDetails_window.setStripDetailsInfo = Label(self.setStripDetails_window, text="Enter a name for the strip and the number of LEDs on it")
        self.setStripDetails_window.setStripDetailsInfo.pack(side="top", fill="both", expand=True, padx=10, pady=10)

        self.setStripDetails_window.Prompt1 = Label( self.setStripDetails_window, text="Strip Name")
        self.setStripDetails_window.Entry1 = Entry(self.setStripDetails_window, bd =5)
        self.setStripDetails_window.Prompt2 = Label(self.setStripDetails_window, text="Number of LEDs")
        self.setStripDetails_window.Entry2 = Entry(self.setStripDetails_window, bd =5)

        self.setStripDetails_window.Prompt1.pack()
        self.setStripDetails_window.Entry1.pack()  
        self.setStripDetails_window.Prompt2.pack()
        self.setStripDetails_window.Entry2.pack()  
            
        #button to connect
        self.setStripDetails_window.createButton = Button(self.setStripDetails_window, text="Create", pady = 10, padx = 10, command=self.createStrip)
        self.setStripDetails_window.createButton.pack(side="bottom")

        #bring to their attention
        raise_above_all(self.setStripDetails_window)

    def editStripDetails_window(self):

        #setup
        self.editStripDetails_window = Toplevel(self)
        self.pack(fill=BOTH, expand=1)
        self.editStripDetails_window.resizable(0,0)
        self.editStripDetails_window.wm_title("Edit strip")
        self.editStripDetails_window.wm_iconbitmap('lights_icon.ico')

        self.editStripDetails_window.editInfo = Label(self.editStripDetails_window, text="Currently editing: " + self.layoutManager_window.stripToEdit)
        self.editStripDetails_window.editInfo.pack(side="top", fill="both", expand=True, padx=10, pady=10)

        self.editStripDetails_window.Prompt1 = Label(self.editStripDetails_window, text="Strip Name")
        self.editStripDetails_window.Entry1 = Entry(self.editStripDetails_window, bd =5)
        self.editStripDetails_window.Prompt2 = Label(self.editStripDetails_window, text="Number of LEDs")
        self.editStripDetails_window.Entry2 = Entry(self.editStripDetails_window, bd =5)

        self.editStripDetails_window.Prompt1.pack()
        self.editStripDetails_window.Entry1.pack()  
        self.editStripDetails_window.Prompt2.pack()
        self.editStripDetails_window.Entry2.pack()    
        #button to connect
        self.editStripDetails_window.editButton = Button(self.editStripDetails_window, text="Ok", pady = 10, padx = 10, command=self.editStrip)
        self.editStripDetails_window.editButton.pack(side="bottom")

        #bring to their attention
        raise_above_all(self.editStripDetails_window)


    ######################################################NONE WINDOW FUNCTIONS###################################################

    def Off(self):
        self.bluetooth_window.sock.send("C")

    def On(self):
        self.bluetooth_window.sock.send("D")

    def smartLights(self):
        self.bluetooth_window.sock.send("E")

    def randomColour(self):
        self.bluetooth_window.sock.send("B")
        R = str(random.randint(0,255)).zfill(3)
        G = str(random.randint(0,255)).zfill(3)
        B = str(random.randint(0,255)).zfill(3)
        self.bluetooth_window.sock.send(R)
        time.sleep(0.1)
        self.bluetooth_window.sock.send(G)
        time.sleep(0.1)
        self.bluetooth_window.sock.send(B)

    #function that runs when option is selected
    def onBluetoothSelect(self, val):
      
        #find name of device
        sender = val.widget
        idx = sender.curselection()
        value = sender.get(idx)

        #match it with an address
        for device in self.bluetooth_window.devices:  
            if(value == device[0]):
                self.bluetooth_window.chosenDeviceName = device[0]
                self.bluetooth_window.chosenDeviceAddress = device[1]

    def connectBluetoothDevice(self):
        target_address = self.bluetooth_window.chosenDeviceAddress
        self.bluetooth_window.sock=BluetoothSocket( RFCOMM )
        self.bluetooth_window.sock.connect((target_address, 1))
        self.bluetooth_window.sock.settimeout(1.0)
        self.bluetooth_window.sock.send("A")

    def removeStrip(self):
        numStrips = self.layoutManager_window.stripList.size()
        for i in range(0,numStrips):
            if(self.layoutManager_window.stripList.get(i) == self.layoutManager_window.stripToRemove):
                self.layoutManager_window.stripList.delete(i)

    def createStrip(self):

        if((self.setStripDetails_window.Entry1.get() == "") or (self.setStripDetails_window.Entry2.get() == "")):
            pass
        else:
            self.layoutManager_window.stripList.insert(END,str(self.setStripDetails_window.Entry1.get()) + " with " + str(self.setStripDetails_window.Entry2.get()) + " Pixels")
            self.setStripDetails_window.destroy()

    def editStrip(self):
        numStrips = self.layoutManager_window.stripList.size()

        for i in range(0,numStrips):
            if(self.layoutManager_window.stripList.get(i) == self.layoutManager_window.stripToEdit):
                self.layoutManager_window.stripList.delete(i)
                self.layoutManager_window.stripList.insert(i,str(self.editStripDetails_window.Entry1.get()) + " with " + str(self.editStripDetails_window.Entry2.get()) + " Pixels")
        self.editStripDetails_window.destroy()

    #function that runs when option is selected
    def onStripSelect(self, val):
      
        sender = val.widget
        idx = sender.curselection()
        value = sender.get(idx)   
        self.layoutManager_window.stripToRemove = value
        self.layoutManager_window.stripToEdit = value

    def saveLayout(self):
        preset = []
        numStrips = self.layoutManager_window.stripList.size()

        for i in range(0,numStrips):
            preset.append(self.layoutManager_window.stripList.get(i))

        print preset


    #opens colour selection screen, output is RGB value, sends it to the board via bluetooth
    def getColor(self):
        self.bluetooth_window.sock.send("B")
        colour = askcolor()
        R = str(colour[0][0]).zfill(3)
        G = str(colour[0][1]).zfill(3)
        B = str(colour[0][2]).zfill(3)
        self.bluetooth_window.sock.send(R)
        time.sleep(0.1)
        self.bluetooth_window.sock.send(G)
        time.sleep(0.1)
        self.bluetooth_window.sock.send(B)

#bring window to front
def raise_above_all(window):

    window.attributes('-topmost', 1)
    window.attributes('-topmost', 0)

def main():

    root = Tk()
    root.state('zoomed')
    root.minsize(width=666, height=666)
    root.maxsize(width=666, height=666)
    root.wm_iconbitmap('lights_icon.ico')
    app = Example(root)
    root.mainloop()  


if __name__ == '__main__':
    main()
