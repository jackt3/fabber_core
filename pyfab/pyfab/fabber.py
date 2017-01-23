import os
import subprocess as sub
import traceback
import distutils.spawn
import nibabel as nib

from ctypes import *
import numpy as np
import numpy.ctypeslib as npct

class LibRun:
    """
    A fabber library run, with output data and logfile
    """
    def __init__(self, data, log):
        self.data = data
        self.log = log

class DirectoryRun:
    """
    A particular run of the fabber executable, with its output
    directory, logfile and output data
    """

    def __init__(self, dir):
        self.dir = dir
        self.files = {}
        self.data = {}

        self.logfile, self.log = self._get_log()
        self.time, self.timestamp = self._get_timestamp()
        self.isquick = self._is_quick_run()

        try:
            self._get_params()
        except:
            print("Failed to get parameters")
            self.params = set()

        self.scan_output()

    def _get_log(self):
        logfile = os.path.join(self.dir, "logfile")
        f = open(logfile)
        log = "".join(f.readlines())
        f.close()
        return logfile, log

    def scan_output(self):
        if len(self.files) > 0: return  # Only do this once?
        try:
            outdir_files = [f for f in os.listdir(self.dir)
                            if os.path.isfile(os.path.join(self.dir, f)) and f.endswith(".nii.gz")]
        except:
            print("Could not read output directory: ", self.dir)
            traceback.print_exc()
            return

        for fname in outdir_files:
            try:
                name = fname.split(".")[0]
                #role, param = self._get_role_from_name(name)
                f = os.path.join(self.dir, fname)
                self.files[name] = f
                d = nib.load(f).get_data()
                self.data[name] = d
            except:
                print("Not a valid output data file: ", fname)

    def _get_timestamp(self):
        prefixes = ["start time:", "fabberrundata::start time:"]
        f = open(self.logfile)
        lines = f.readlines()
        f.close()
        for line in lines:
            l = line.strip()
            for prefix in prefixes:
                if l.lower().startswith(prefix):
                    timestamp = l[len(prefix):].strip()
                    try:
                        time = datetime.datetime.strptime(self.timestamp, "%c")
                    except:
                        print("Failed to parse timestamp")
                        time = os.path.getmtime(self.logfile)
                    return time, timestamp
        raise Exeception("Could not find timestamp in log file")

    def _is_quick_run(self):
        return os.path.isfile(os.path.join(self.dir, "QUICKRUN.txt"))

    def _get_role_from_name(self, name):
        """ Get file role (human readable unique description of
            what it contains) from its filesname"""
        role = name
        param = None
        if name.startswith("mean_"):
            role = name[5:] + " Mean value"
            param = name[5:]
        elif name.startswith("std_"):
            role = name[4:] + " Std. dev."
            param = name[4:]
        elif name.startswith("zstat_"):
            role = name[6:] + " Zstat"
            param = name[6:]
        elif name.startswith("modelfit"):
            role = "Model prediction"
        elif name.startswith("residuals"):
            role = "Model residuals"

        print name, role, param
        return role, param

    def _get_params(self):
        f = open(os.path.join(self.dir, "paramnames.txt"), "r")
        self.params = set([p.strip() for p in f.readlines()])
        f.close()


class FabberExec:
    """
    Encapsulates a Fabber executable context and provides methods
    to query models and options and also run a file
    """
    def __init__(self, rundata, ex=None):
        self.rundata = rundata
        if ex:
            self._ex = ex
        else:
            self._ex = self._find_fabber()

    def set_exec(self, ex):
        """
        Set the executable
                
        FIXME should check if it seems valid
        """
        self._ex = ex

    def get_exec(self):
        return self._ex
                
    def get_methods(self):
        """ Get known inference methods """
        stdout = self._run_help("--listmethods")
        return [line.strip() for line in stdout.split()]

    def get_models(self):
        """ Get known models """
        stdout = self._run_help("--listmodels")
        return [line.strip() for line in stdout.split()]
  
    def get_options(self, method=None, model=None):
        """
        Get general options, or options for a method/model. Returns a list of dictionaries
        with keys: name, type, optional, default, description
        """
        args = []
        if method: args.append("--method=%s" % method)
        if model: args.append("--model=%s" % model)
        stdout = self._run_help("--help", *args)
        lines = stdout.split("\n")
        opts = []
        desc = ""
        descnext = False
        for line in lines:
            if line.startswith("--"):
                line2 = line[2:].replace("[", ",").replace("]", "")
                l = [o.strip() for o in line2.split(",")]
                if len(l) < 4: 
                    continue
                opt = {}
                opt["name"] = l[0]
                opt["type"] = l[1]
                opt["optional"] = l[2] != "REQUIRED"
                if l[3].startswith("DEFAULT="): opt["default"] = l[3].split("=", 1)[1]
                else: opt["default"] = ""
                if opt["name"] in ["data", "data<n>", "mask", "help", "model", "method", "listmodels", "listmethods"]:
                    continue
                if len(l) == 4:
                    opts.append(opt)
                    descnext = True
                else: 
                    descnext = False
                    print("Couldn't _parse option: " + line)
            elif descnext:
                opt["description"] = line.strip()
                descnext = False
            elif not line.startswith("Usage") and not line.startswith("Options") and not line.startswith("Description"):
                desc += line.strip()

        #print opts
        return opts, desc

    def run(self):
        """
        Run Fabber on the run data specified
        """
        self.rundata.save()
        workdir  = self.rundata.get_filedir()
        cmd = [self._ex, "-f", self.rundata.filepath]
        #print(cmd)
        err = ""
        p = sub.Popen(cmd, stdout=sub.PIPE, stderr=sub.PIPE, cwd=workdir)
        while 1:
            (stdout, stderr) = p.communicate()
            status = p.poll()
            #print stdout
            if stderr: err += stderr
            if status is not None: break
            
            if status != 0:
                raise Exception(err)

        # Hack to get the last run
        return self.rundata.get_runs()[0]

    def _write_temp_mask(self):
        pass
    """ FIXME
        fname = os.path.join(self.fabdir, "fabber_mask_temp.nii.gz")
        data = np.zeros(self.shape[:3])
        data[self.focus[0], self.focus[1], self.focus[2]] = 1
        affine = self.data["data"].affine # FIXME
        img = nib.Nifti1Image(data, affine)
        nib.save(img, fname)
        return fname
    """
            
    def _write_quickrun_file(self, dir):
        """
        Write a little file which identifies a particular
        run as a 1-voxel test run
        """
        f = open(os.path.join(dir, "QUICKRUN.txt"), "wc")
        f.write("This data is from a 1-voxel test run\n")
        f.close()
 
        
    def _run_help(self, *opts):
        """
	    Run Fabber synchronously, this is presumed to be a quick
	    run in order to get help options.

	    Does not throw on failure because user may need to set
	    the location of the executable first
	    """
        cmd = [self._ex] + list(opts)
        print cmd
        try:
            p = sub.Popen(cmd, stdout=sub.PIPE)
            (stdout, stderr) = p.communicate()
            status = p.wait()
            if status == 0:
                return stdout
            else:
                print("Failed to run fabber: " + str(cmd))
            return ""
        except:
            print("Failed to run fabber: " + str(cmd))
            traceback.print_exc()
            return ""
 
    def _find_fabber(self):
        """ 
   	    Find a fabber executable
   	     
   	    FIXME need to do better than this!
   	    """
        guesses = [os.path.join(os.environ["FSLDIR"], "bin/fabber"),
   	                distutils.spawn.find_executable("fabber"),
   	                "fabber"]
        for guess in guesses:
            print guess
            if os.path.isfile(guess):
                return guess

class FabberLib:
    """
    Interface to Fabber in library mode using simplified C-API
    """
    def __init__(self, rundata, lib="/home/martinc/dev/fabber_core/Debug/libfabbercore_shared.so"):
        self.rundata = rundata
        self.lib = CDLL(lib)

        self.errbuf = create_string_buffer(255)
        self.outbuf = create_string_buffer(10000)
        
        # Signatures of the C functions                                                   
        c_int_arr = npct.ndpointer(dtype=np.int32, ndim=1, flags='CONTIGUOUS')
        c_float_arr = npct.ndpointer(dtype=np.float32, ndim=1, flags='CONTIGUOUS')
        
        self.lib.fabber_new.argtypes = [c_char_p]
        self.lib.fabber_new.restype = c_void_p
        self.lib.fabber_load_models.argtypes = [c_void_p, c_char_p, c_char_p]
        self.lib.fabber_set_extent.argtypes = [c_void_p, c_int, c_int, c_int, c_int_arr, c_char_p]
        self.lib.fabber_set_opt.argtypes = [c_void_p, c_char_p, c_char_p, c_char_p]
        self.lib.fabber_set_data.argtypes = [c_void_p, c_char_p, c_int, c_float_arr, c_char_p]
        self.lib.fabber_get_data_size.argtypes = [c_void_p, c_char_p, c_char_p]
        self.lib.fabber_get_data.argtypes = [c_void_p, c_char_p, c_float_arr, c_char_p]
        self.lib.fabber_dorun.argtypes = [c_void_p, c_int, c_char_p, c_char_p]
        self.lib.fabber_destroy.argtypes = [c_void_p]
     
        self.lib.fabber_get_options.argtypes = [c_void_p, c_char_p, c_char_p, c_int, c_char_p, c_char_p]
        self.lib.fabber_get_models.argtypes = [c_void_p, c_int, c_char_p, c_char_p]
        self.lib.fabber_get_methods.argtypes = [c_void_p, c_int, c_char_p, c_char_p]

        self.handle = self.lib.fabber_new(self.errbuf)
        if self.handle == 0:
            raise Exception("Error creating fabber context (%s)" % self.errbuf.value)

        if rundata.options.has_key("loadmodels"):
            self._trycall(self.lib.fabber_load_models, self.handle, rundata.options["loadmodels"], self.errbuf)
        for key, value in rundata.options.items():
            print key, value
            self._trycall(self.lib.fabber_set_opt, self.handle, key, value, self.errbuf)

    def __del__(self):
        handle = getattr(self, "handle")
        if handle is not None: self.lib.fabber_destroy(handle)

    def _trycall(self, call, *args):
        ret = call(*args)
        if ret < 0:
            raise Exception("Error in native code: %i (%s)" % (ret, self.errbuf.value))
        else:
            return ret

    def run(self):
        nx, ny, nz = -1, -1, -1
        mask = None
        data = {}
        for key, value in self.rundata.options.items():
            try:
                f = nib.load(value)
                d = f.get_data()
                if nx == -1:
                    nx, ny, nz = d.shape[:3]
                else:
                    # Check consistent shape?
                    pass
                if key == "mask":
                    mask = d
                else:
                    data[value] = d
            except:
                if key == "data":
                    raise
                # Otherwise ignore, most options will not be data files
                pass

        return self.run_with_data(nx, ny, nz, mask, data, [])

    def run_with_data(self, nx, ny, nz, mask, data, output_items):
        """
        Run fabber

        :param nx: Data extent in x direction
        :param ny: Data extent in y direction
        :param nz: Data extent in z direction
        :param mask: Mask as Numpy array, or None if no mask
        :param data: Dictionary of data: string key, Numpy array value
        :param output_items: List of names of data items to return
        :return: Tuple of 1. Dictionary of output data, string key, Numpy array value and 2. log
        """
        nv = nx*ny*nz

        if mask is None: mask = np.ones(nv)
        # Make suitable for passing to int* c function
        mask = np.ascontiguousarray(mask.flatten(), dtype=np.int32)

        self._trycall(self.lib.fabber_set_extent, self.handle, nx, ny, nz, mask, self.errbuf)
        for key, item in data.items():
            if len(item.shape) == 3: size=1
            else: size = item.shape[3]
            item = np.ascontiguousarray(item.flatten(), dtype=np.float32)
            self._trycall(self.lib.fabber_set_data, self.handle, key, size, item, self.errbuf)
                
        self._trycall(self.lib.fabber_dorun, self.handle, len(self.outbuf), self.outbuf, self.errbuf)

        retdata = {}
        for key in output_items:
            size = self._trycall(self.lib.fabber_get_data_size, self.handle, key, self.errbuf)
            arr = np.ascontiguousarray(np.empty(nv*size, dtype=np.float32))
            self._trycall(self.lib.fabber_get_data, self.handle, key, arr, self.errbuf)
            arr = np.squeeze(arr.reshape([nx,ny,nz,size]))
            retdata[key] = arr

        return LibRun(retdata, self.outbuf.value)
              
    def get_options(self, method=None, model=None):
        if method:
            key = "method"
            value = method
        elif model:
            key = "model"
            value = model
        else:
            key = None
            value = None
        self._trycall(self.lib.fabber_get_options, self.handle, key, value, len(self.outbuf), self.outbuf, self.errbuf)
        opt_keys = ["name", "description", "type", "optional", "default"]
        opts = []
        for opt in self.outbuf.value.split("\n"):
            if len(opt) > 0:
                opt = dict(zip(opt_keys, opt.split("\t")))
                opt["optional"] = opt["optional"] == "1"
                opts.append(opt)
        return opts, ""
                
    def get_methods(self):
        """ Get known inference methods"""
        self._trycall(self.lib.fabber_get_methods, self.handle, len(self.outbuf), self.outbuf, self.errbuf)
        return self.outbuf.value.splitlines()

    def get_models(self):
        """ Get known models"""
        self._trycall(self.lib.fabber_get_models, self.handle, len(self.outbuf), self.outbuf, self.errbuf)
        return self.outbuf.value.splitlines()

    def get_params(self, opts):
        """ Get the model parameters, given the specified options"""
        self._trycall(self.lib.fabber_get_model_params, self.handle, len(self.outbuf), self.outbuf, self.errbuf)
        return self.outbuf.value.splitlines()