#ifndef QED_TABLES_COMMONS_
#define QED_TABLES_COMMONS_

#include <iostream>
#include <string>

template <typename ContainerType, typename ElementType>
bool Contains (const ContainerType& container, const ElementType& el)
{
    return container.find(el) != std::end(container);
}

void AbortWithMessage(const std::string& msg)
{
    std::cerr << "### ABORT : " << msg << "\n";
    std::cerr << "___________________________\n";
    exit(1);
}

void SuccessExit()
{
    std::cout << "___________________________\n";
    exit(0);
}

#endif //QED_TABLES_COMMONS_
