#!/usr/bin/env python
#!/bin/env python
#
import urllib
import os
import ftplib
import httplib
from exceptions import *
from sys import *
from string import *
from string import *
from time import *
from urlparse import *
from string   import *
import commands
import re
import sys

#=========================Handles getting files =================================================
def extension(filename):
    return split(filename,'.')[-1]

def basename(filename):
    return join(split(filename,'.')[0:-1],'.')

def uncompress(filename):
    ext = extension(filename)
    if ext == 'gz':
        err = os.system('gunzip ' + filename)
        if err != 0:
            print 'Error unable to invoke gunzip on ' + filename
            sys.exit()
    elif ext == 'Z':
        err = os.system('uncompress ' + filename)        
        if err != 0:
            print 'Error unable to invoke uncompress on ' + filename
            sys.exit()

def compressed(filename):
    ext = extension(filename)
    if ext == 'gz' or ext == 'Z':
        return 1
    else:
        return 0
    
# Defines a meta class, whose member functions are common/required
# by ftp/http object classes
class url_object:
    def gettime(): 
        print 'Error Derived function should be implemented'
        sys.exit()
    def getfile(filename):
        print 'Error Derived function should be implemented'
        sys.exit()

class local_object(url_object):
    def __init__(self,machine,urlpath):
        self.localfilename = urlpath
        if os.path.isfile(self.localfilename) == 0:
            print 'Error! file:',self.localfilename,' does not exist'
            sys.exit()
            
    def gettime(self):
        return os.stat(self.localfilename)[7]

    def getfile(self,outfilename):
        fp_in  = open(self.localfilename,'r')
        fp_out = open(outfilename,'w')
        fp_out.write(fp_in.read())
        fp_in.close()
        fp_out.close()

class ftp_object(url_object):
    def __init__(self,machine,urlpath):
        self.machine = machine
        self.urlpath = urlpath
        self.buf     = ''
        try :
            self.ftp     = ftplib.FTP(self.machine)
        except:
            print 'Error! accessing server',self.machine
            sys.exit()
            
        self.ftp.login()

    def __del__(self):
        self.ftp.close()

    def readftplines(self,buf1):
        self.buf   = self.buf + buf1

    def gettime(self):
        
        self.buf       = ''
        self.ftp.retrlines('LIST ' +self.urlpath,self.readftplines)
        if self.buf == '':
            print 'Error! file does not exist on the server'
            self.ftp.close()
            sys.exit()

        month,day,year = split(self.buf)[5:8]
        hour,min       = '0','0'

        if len(split(year,':')) >=2:
            hour,min   = split(year,':')
            year       = gmtime(time())[0]
        else:
            year = atoi(year)

        month_d = {'Jan':1,'Feb':2,'Mar':3,'Apr':4,'May':5,'Jun':6,
                   'Jul':7,'Aug':8,'Sep':9,'Oct':10,'Nov':11,'Dec':12}

        newtime = mktime((year,month_d[month],atoi(day),atoi(hour),
                          atoi(min),0,-1,-1,0))
        c_time  = time()
        if newtime > c_time:
            newtime = mktime((year-1,month_d[month],atoi(day),
                              atoi(hour),atoi(min),0,-1,-1,0))
        self.remotetime = newtime - timezone
        return self.remotetime
    
    def writefile(self,buf):
        self.fp.write(buf)

    def getfile(self,outfilename):
        self.fp  = open(outfilename,'w')
        self.ftp.retrbinary('RETR '+ self.urlpath,self.writefile)
        self.fp.close()


class http_object(url_object):
    def __init__(self,machine,urlpath):
        self.machine = machine
        self.urlpath = urlpath
        try:
            self.http = httplib.HTTP(self.machine)
        except:
            print 'Error! accessing server',self.machine
            sys.exit()
        
        self.http.putrequest('GET',self.urlpath)
        self.http.putheader('Accept','*/*')
        self.http.endheaders()
        errcode, errmesg, self.headers = self.http.getreply()
        if errcode != 200:
            print 'Error! Accessing url on the server.',errcode,errmesg
            self.http.close()
            sys.exit()
        filesize   = self.headers.dict['content-length']
        if filesize < 2000 :
            print 'Error! Accessing url on the server. bytes-received :',filesize
            self.http.close()
            sys.exit()

    def __del__(self):
        self.http.close()


    def gettime(self):

        # Get the remote timestamps
        urltimestamp = self.headers.dict['last-modified']
        month_d             = {'Jan':1,'Feb':2,'Mar':3,'Apr':4,'May':5,'Jun':6,
                               'Jul':7,'Aug':8,'Sep':9,'Oct':10,'Nov':11,'Dec':12}

        if len(split(urltimestamp)) == 6 :      #Sun, 06 Nov 1994 08:49:37 GMT
            day,month,year,time = split(urltimestamp)[1:-1] 
        elif len(split(urltimestamp)) == 4 :    #Sunday, 06-Nov-94 08:49:37 GMT
            time           = split(urltimestamp)[2] 
            day,month,year = split(split(urltimestamp)[1],'-')
        else :                                  #Sun Nov  6 08:49:37 1994
            month,day,time,year = split(urltimestamp)[1:]

        hour,min,sec        = split(time,':')
        newtime             = (atoi(year),month_d[month],atoi(day),atoi(hour),
                               atoi(min),atoi(sec),-1,-1,0)
        self.remotetime     = mktime(newtime) - timezone
        return self.remotetime

    def getfile(self,outfilename):
        #read the data
        f    = self.http.getfile()
        data = f.read()
        f.close()
        # Now write this data to a file
        fp = open(outfilename,'w')
        fp.write(data)
        fp.close()
        
class urlget:

    def __init__(self,url,filename ='',tmpdir='/tmp'):
        self.url                                = urlunparse(urlparse(url))
        self.protocol,self.machine,self.urlpath = urlparse(self.url)[0:3]
        self.compressed = 0
        self.cachefilename = 0

        # Uncompress is not done if the filename is provided
        if filename != '':
            self.cache         = 1
            self.filename      = filename
            self.cachefilename = filename
        else:
            self.cache      = 1
            self.filename   = tmpdir+'/'+replace(join(urlparse(self.url)[0:3],'@'),'/','_')
            self.compressed = compressed(self.filename)
            if self.compressed == 1:
                self.cachefilename = basename(self.filename)
            else:
                self.cachefilename = self.filename
                
        if self.protocol == 'ftp':
            self.url_obj = ftp_object(self.machine,self.urlpath)
        elif self.protocol == 'http':
            self.url_obj = http_object(self.machine,self.urlpath)
        else:
            # Assume local file copy
            self.url_obj = local_object(self.machine,self.urlpath)
            #print 'Error! Unknown protocol. use ftp or http protocols only'
            #sys.exit()
        timestamp = self.url_obj.gettime()
        uselocalcopy = 0
        if os.path.isfile(self.cachefilename) == 1:
            mtime = os.stat(self.cachefilename)[7]
            if mtime >= timestamp:
                uselocalcopy = 1

        if self.cache == 0 and os.path.isfile(self.cachefilename) == 1:
            flag = 0
            while flag == 0:
                print self.filename,'exists. Would you like to replace it? (y/n)'
                c = stdin.readline()[0]
                if c == 'y': 
                    uselocalcopy = 0
                    flag = 1
                elif c == 'n':
                    uselocalcopy = 1
                    flag = 1
                    
        if uselocalcopy == 0 :
            self.url_obj.getfile(self.filename)
            os.utime(self.filename,(timestamp,timestamp))
            if self.compressed == 1:
                uncompress(self.filename)
            os.chmod(self.cachefilename,500)

#========================================================================================
def checkcxxcompiler():
    (status,output) = commands.getstatusoutput("g++ -dumpversion")
    if not status == 0:
        print "g++ is not in your path; please make sure that you have a g++"
        print "of at least version 3 installed in your path"
        print "Get gcc/g++ at http://gcc.gnu.com"
        return 0
    if not re.split('\.',output)[0] == "3":
        print "The g++ in your path is not of version 3 or higher; please install a g++"
        print "of at least version 3 or fix your path"
        print "Get gcc/g++ at http://gcc.gnu.com"
        (status,output) = commands.getstatusoutput("g++ --version")
        print output
        return 0
    return 1

def checkpython():
    if not hasattr(sys,"version_info") or float(sys.version_info[0]) < 2 or float(sys.version_info[1]) < 2:
        print "Requires Python version 2.2 or higher"
        print "Get Python at python.org"
        return 0
    return 1


def getjavainclude():
#    return "[/home/petsc/software/j2sdk1.4.0-linux/include/linux/,/home/petsc/software/j2sdk1.4.0-linux/include/]"
     return ''

def getjavalib():
#    return "/home/petsc/software/j2sdk1.4.0-linux/jre/lib/i386/client/libjvm.so"
     return ''
 
#==================================================================================
def main():
    if checkcxxcompiler() == 0: return
    if checkpython() == 0: return
    
    logfile = open("logfile",'w')
    
    srcdir = os.getcwd()
    print "Directory to compile source code (hit return for current) "+srcdir
    c = stdin.readline()
    if not len(c) == 1:
      if c[0] == '/':
         srcdir = c[:-1]
      else:
         srcdir = srcdir+c[:-1]
    print "Compiling in directory: "+srcdir
    try:
      os.makedirs(srcdir)
    except:
      pass

    installdir = os.getcwd()
    print "Directory to install (hit return for current) "+installdir
    c = stdin.readline()
    if not len(c) == 1:
      if c[0] == '/':
         installdir = c[:-1]
      else:
         installdir = installdir+c[:-1]
    print "Intalling in directory: "+installdir
    try:
      os.makedirs(installdir)
    except:
      pass

    JAVA_INCLUDE = getjavainclude()
    JAVA_LIB = getjavalib()
    
    if os.environ.has_key('TMPDIR'):
      tmpdir =  os.environ['TMPDIR']+"/"
      try:
          os.makedirs(tmpdir)
      except:
          pass
    else:
      tmpdir = "/tmp/"

    print "Retreiving build system"
    x = urlget("ftp://info.mcs.anl.gov/pub/petsc/sidl/bs.tar.gz",tmpdir+"bs.tar.gz",tmpdir)
    (status,output) = commands.getstatusoutput("cd "+srcdir+";tar -zxf "+tmpdir+"bs.tar.gz")
    logfile.write(output)
    if not status == 0:
        print "Failed extracting bs"
        print output
        return
    
    print "Initializing the database in the build system"
    (status,output) = commands.getstatusoutput("cd "+srcdir+"/bs;./make.py -debugLevel=0 -debugSections=[] -restart=0 -SIDLRUNTIME_DIR="+srcdir+"/SIDLRuntimeANL -JAVA_INCLUDE="+JAVA_INCLUDE+" -JAVA_RUNTIME_LIB="+JAVA_LIB+" -installh="+installdir+"/include -installlib="+installdir+"/lib -installexamples="+installdir+"/examples printTargets")
    logfile.write(output)
    if not status == 0:
        print "Failed to initialize build system database"
        print output
        return
    
    if os.environ.has_key('PYTHONPATH'):
        os.environ['PYTHONPATH'] = srcdir+"/bs:"+installdir+"/lib:"+os.environ['PYTHONPATH']
    else:
        os.environ['PYTHONPATH'] = srcdir+"/bs:"+installdir+"/lib"
        
    print "Retreiving runtime system"
    x = urlget("ftp://info.mcs.anl.gov/pub/petsc/sidl/SIDLRuntimeANL.tar.gz",tmpdir+"SIDLRuntimeANL.tar.gz",tmpdir)
    (status,output) = commands.getstatusoutput("cd "+srcdir+";tar -zxf "+tmpdir+"SIDLRuntimeANL.tar.gz")
    logfile.write(output)
    if not status == 0:
        print "Failed extracting SIDLRuntimeANL"
        print output
        return
    
    print "Compiling runtime system"
    (status,output) = commands.getstatusoutput("cd "+srcdir+"/SIDLRuntimeANL;./make.py -install=1 compile")
    logfile.write(output)
    if not status == 0:
        print "Failed compiling SIDLRuntimeANL"
        print output
        return
    
    print "Compiling build system"
    (status,output) = commands.getstatusoutput("cd "+srcdir+"/bs;./make.py -install=1 compile")
    logfile.write(output)
    if not status == 0:
        print "Failed compiling the build system"
        print output
        return
    
    print "Retreiving GUI system"
    x = urlget("ftp://info.mcs.anl.gov/pub/petsc/sidl/gui.tar.gz",tmpdir+"gui.tar.gz",tmpdir)
    (status,output) = commands.getstatusoutput("cd "+srcdir+";tar -zxf "+tmpdir+"gui.tar.gz")
    logfile.write(output)
    if not status == 0:
        print "Failed extracting GUI"
        print output
        return
    
    print "Compiling GUI system"
    (status,output) = commands.getstatusoutput("cd "+srcdir+"/gui;./make.py -install=1 compile")
    logfile.write(output)
    if not status == 0:
        print "Failed compiling GUI"
        print output
        return

    print "Running installer"
    (status,output) = commands.getstatusoutput(installdir+"/examples/python/installer.py")
    logfile.write(output)
    if not status == 0:
        print "Failed running installer"
        print output
        return

    print "Set your PYTHONPATH to "+installdir+"/lib and try the examples in "+installdir+"/examples/[python,c++]"
    logfile.close()
# The classes in this file can also
# be used in other python-programs by using 'import'
if __name__ ==  '__main__': 
    main()

