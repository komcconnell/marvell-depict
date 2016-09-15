#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <linux/kdev_t.h>
#include <sysutils/NetlinkEvent.h>
#include <cstdlib>
#include <sstream>

#define LOG_TAG "MediaVolume"
#include <cutils/log.h>

#include "VolumeManager.h"
#include "ResponseCode.h"
#include "cryptfs.h"
#include "Fat.h"
#include "MVmgr.h"

static int createDeviceNode(const char *path, int major, int minor);

/*
 * Class: MVinfo
 */
MVinfo::MVinfo(std::string inLabel, std::string inMountPoint, std::string inDevPath) {
    mLabel = inLabel;
    mMountPoint = inMountPoint;
    mDevicePath = inDevPath;
    mMajor = 0;
    mMinorTotal = 0;
    mMinorValid = 0;
}

bool MVinfo::isManaged(std::string inDevPath) {
    if (0 == mDevicePath.compare(0, mDevicePath.length(), inDevPath, 0, mDevicePath.length())) {
        return true;
    } else {
        return false;
    }
}

void MVinfo::dump(void) {
   SLOGD("MVinfo[%s,%s,%s, M=%d,m1=%d,m2=%d]", mLabel.c_str(), mMountPoint.c_str(), mDevicePath.c_str(), mMajor, mMinorTotal, mMinorValid);
}

/*
 * Class: MVmgr
 */
MVmgr::MVmgr(VolumeManager *vm) {
    mVM = vm;
    mMVinfoCollection = new MVinfoCollection();
}

MVmgr::~MVmgr() {
}

int MVmgr::handleBlockEvent(NetlinkEvent *evt) {
    int ret = 0;
    const char *dp = evt->findParam("DEVPATH");
    const char *devtype = evt->findParam("DEVTYPE");
    int major = atoi(evt->findParam("MAJOR"));
    int minor = atoi(evt->findParam("MINOR"));
    const char *devname = evt->findParam("DEVNAME");
    int action = evt->getAction();
    bool isMV = this->isManaged(dp);
    if (isMV) {
//        SLOGD("%s is managed", dp);
        ret = 0;
    } else {
        ret = -1;
        return ret;
    }
    std::ostringstream ss;
    ss << "/dev/block/vold/" << major << ":" << minor;

    std::string nodepath = ss.str();

    /* We can handle this disk */
    if (action == NetlinkEvent::NlActionAdd) {
        bool bNeedCreateMV = false;
        if (!strcmp(devtype, "disk")) {
            const char *nParts = evt->findParam("NPARTS");
            if (!strcmp("0", nParts)) {
                bNeedCreateMV = true;
            }
        } else if (!strcmp(devtype, "partition")) {
            bNeedCreateMV = true;
        }

        if (bNeedCreateMV) {
            MediaVolume *item = NULL;
            if (createDeviceNode(nodepath.c_str(), major, minor)) {
                SLOGE("Error making device node '%s' (%s)", nodepath.c_str(), strerror(errno));
                return ret;
            }

            char uuid[256];
            char fstype[256];
            char label[256];
            if (EXIT_SUCCESS == blkutil_get_volume_info(nodepath.c_str(), uuid, fstype, label)) {
                char* newLabel = NULL;
                std::string aMountPoint = "/mnt/media/usb." + std::string(uuid);
                SLOGD("%s,%s,%s,%s", nodepath.c_str(), uuid, fstype, label);

                if (0 == strlen(label)) {
                    newLabel = strdup(devname);
                } else {
                    newLabel = urlencode(label);
                }

                fstab_rec *aRec = new fstab_rec();
                aRec->label = newLabel;
                aRec->mount_point = strdup(aMountPoint.c_str());
                aRec->fs_type = fstype;
                item = new MediaVolume(mVM, aRec, 0, major, minor);
                mVM->addVolume(item);
                char msg[255];
                snprintf(msg, sizeof(msg), "Volume %s %s disk inserted (%d:%d)", newLabel,
                         aMountPoint.c_str(), major, minor);
                mVM->getBroadcaster()->sendBroadcast(ResponseCode::VolumeDiskInserted, msg, false);
                free(newLabel);
                newLabel = NULL;
                free(aRec->mount_point);
                delete aRec;
            } else {
                SLOGE("blkutil fail to probe %s", nodepath.c_str());
            }
        } else {
            //pass
        }
    } else if (action == NetlinkEvent::NlActionRemove) {
        bool bNeedDeleteMV = false;
        if (!strcmp(devtype, "disk")) {
            const char *nParts = evt->findParam("NPARTS");
            if (!strcmp("0", nParts)) {
            //(1)No part available on disk (2)No part exists on disk
                bNeedDeleteMV = true;
            }
        } else if (!strcmp(devtype, "partition")) {
            bNeedDeleteMV = true;
        }

        if (bNeedDeleteMV) {
            unlink(nodepath.c_str());
            MediaVolume* item = (MediaVolume*) mVM->getVolume(major, minor);
            if (item) {
                char msg[255];
                int state = item->getState();
                if (state == Volume::State_Mounted) {
                    snprintf(msg, sizeof(msg), "Volume %s %s bad removal (%d:%d)",
                             item->getLabel(), item->getMountpoint(), major, minor);
                    mVM->getBroadcaster()->sendBroadcast(ResponseCode::VolumeBadRemoval, msg, false);
                    if (item->unmountVol(true, false)) {
                        SLOGE("Failed to unmount volume on bad removal (%s)", strerror(errno));
                    }
                }

                snprintf(msg, sizeof(msg), "Volume %s %s disk removed (%d:%d)",
                         item->getLabel(), item->getMountpoint(), major, minor);
                mVM->getBroadcaster()->sendBroadcast(ResponseCode::VolumeDiskRemoved, msg, false);
            } else {
                //pass
            }
            if (mVM->removeVolume(major, minor)) {
#ifdef V_VERBOSE
                mVM->dump();
#endif
            }
        } else {
            //pass
        }
    } else if (action == NetlinkEvent::NlActionChange) {
    } else {
            SLOGW("Ignoring non add/remove/change event");
    }

    return ret;
}

void MVmgr::addDevPath(std::string inLabel, std::string inMountPoint, std::string inDevPath) {
    SLOGD("%s, %s, %s", inLabel.c_str(), inMountPoint.c_str(), inDevPath.c_str());
    int retPrepare = this->prepareMountBase(inMountPoint);
    if (EXIT_SUCCESS != retPrepare) {
        return;
    }
    MVinfo item = MVinfo(inLabel, inMountPoint, inDevPath);
    mMVinfoCollection->push_back(item);
}

bool MVmgr::isManaged(std::string inDevPath) {
    MVinfoCollection::iterator it;
    for (it = mMVinfoCollection->begin(); it != mMVinfoCollection->end(); ++it) {
        if (it->isManaged(inDevPath)) {
            return true;
        }
    }
    return false;
}

void MVmgr::dump() {
    MVinfoCollection::iterator it;
    for (it = mMVinfoCollection->begin(); it != mMVinfoCollection->end(); ++it) {
        it->dump();
    }
}

static int bitCounter(unsigned int n)
{
    unsigned int c = 0;
    for (c = 0; n; n >>= 1) {
        c += n&1;
    }
    return c;
}       /*   -----  end of function bitCounter  ----- */

static int createDeviceNode(const char *path, int major, int minor) {
    mode_t mode = 0660 | S_IFBLK;
    dev_t dev = (major << 8) | minor;
    if (mknod(path, mode, dev) < 0) {
        if (errno != EEXIST) {
            return -1;
        }
    }
    return 0;
}

int MVmgr::prepareMountBase(std::string mountBase) {
    if (access(mountBase.c_str(), R_OK | W_OK | X_OK)) {//mountBase is not writable, mount 'tmpfs' on it
        char mountOptions[255];
        sprintf(mountOptions, "size=64k,uid=%d,gid=%d,mode=%o",
            AID_ROOT, AID_ROOT, 0777);
        if (mount("tmpfs", mountBase.c_str(), "tmpfs", 0, mountOptions)) {
            SLOGE("prepareMountBase: Failed to mount %s as tmpfs, reason=(%s)", mountBase.c_str(), strerror(errno));
            return EXIT_FAILURE;
        } else {
#ifdef V_VERBOSE
            SLOGD("prepareMountBase: mount(tmpfs, %s, tmpfs, 0, %s) success", mountBase.c_str(), mountOptions);
#endif
        }
    } else {
#ifdef V_VERBOSE
        SLOGD("prepareMountBase: %s is already RWX");
#endif
    }

    return EXIT_SUCCESS;
}

int MVmgr::umountVolume(std::string /*inMountPoint*/, MediaVolume* /*inVolume*/) {
    return 0;
}
