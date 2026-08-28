#include "AltSystemParameters.h"

AltSystemParameters::AltSystemParameters() :
    m_moduleCount(1)
{
}

int AltSystemParameters::moduleCount() const
{
    return m_moduleCount;
}

void AltSystemParameters::setModuleCount(int value)
{
    m_moduleCount = value;
}
