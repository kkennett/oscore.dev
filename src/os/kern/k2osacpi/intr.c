#include "k2osacpi.h"

ACPI_STATUS
AcpiOsInstallInterruptHandler(
    UINT32                  InterruptNumber,
    ACPI_OSD_HANDLER        ServiceRoutine,
    void                    *Context)
{
    K2OSKERN_INTR_CONFIG    config;
    K2LIST_LINK *           pListLink;
    K2OSACPI_INTR *         pIntr;

    K2_ASSERT(gK2OSACPI_IntrFreeList.mNodeCount > 0);

    pListLink = gK2OSACPI_IntrFreeList.mpHead;

    K2LIST_Remove(&gK2OSACPI_IntrFreeList, pListLink);

    pIntr = K2_GET_CONTAINER(K2OSACPI_INTR, pListLink, ListLink);

    pIntr->InterruptNumber = InterruptNumber;
    pIntr->ServiceRoutine = ServiceRoutine;
    pIntr->mToken = NULL;

    K2LIST_AddAtTail(&gK2OSACPI_IntrList, pListLink);

    K2MEM_Zero(&config, sizeof(config));

    config.mSourceId = InterruptNumber;
    config.mDestCoreIx = 0;
    config.mIsLevelTriggered = TRUE;
    config.mIsActiveLow = TRUE;

    pIntr->mToken = K2OSKERN_InstallIntrHandler(&config, ServiceRoutine, Context);
    K2_ASSERT(pIntr->mToken != NULL);

    return AE_OK;
}

ACPI_STATUS
AcpiOsRemoveInterruptHandler(
    UINT32                  InterruptNumber,
    ACPI_OSD_HANDLER        ServiceRoutine)
{
    K2_ASSERT(0);
    return AE_ERROR;
}

