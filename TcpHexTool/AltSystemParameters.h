#ifndef ALTSYSTEMPARAMETERS_H
#define ALTSYSTEMPARAMETERS_H

// Alt sistem test parametrelerini tutar.
// Sadece veri tutar; paket olusturmaz, haberlesme yapmaz.
class AltSystemParameters
{
public:
    AltSystemParameters();

    int moduleCount() const;
    void setModuleCount(int value);

private:
    int m_moduleCount;
};

#endif // ALTSYSTEMPARAMETERS_H
