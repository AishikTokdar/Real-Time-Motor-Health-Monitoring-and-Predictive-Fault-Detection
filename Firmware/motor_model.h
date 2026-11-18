#pragma once
#include <cstdarg>
namespace Eloquent {
    namespace ML {
        namespace Port {
            class DecisionTree {
                public:
                    int predict(float *x) {
                        if (x[6] <= 106.19999694824219) {
                            return 3;
                        }

                        else {
                            if (x[7] <= 59.8700008392334) {
                                if (x[2] <= 0.29500000178813934) {
                                    if (x[3] <= 0.23499999940395355) {
                                        return 0;
                                    }

                                    else {
                                        if (x[3] <= 0.24499999731779099) {
                                            if (x[6] <= 1320.3400268554688) {
                                                return 0;
                                            }

                                            else {
                                                return 1;
                                            }
                                        }

                                        else {
                                            if (x[5] <= 0.13499999791383743) {
                                                return 1;
                                            }

                                            else {
                                                return 1;
                                            }
                                        }
                                    }
                                }

                                else {
                                    if (x[7] <= 38.21999931335449) {
                                        return 0;
                                    }

                                    else {
                                        if (x[3] <= 0.23499999940395355) {
                                            if (x[7] <= 42.06500053405762) {
                                                return 4;
                                            }

                                            else {
                                                return 4;
                                            }
                                        }

                                        else {
                                            if (x[7] <= 41.53000068664551) {
                                                return 4;
                                            }

                                            else {
                                                return 4;
                                            }
                                        }
                                    }
                                }
                            }

                            else {
                                return 2;
                            }
                        }
                    }

                protected:
                };
            }
        }
    }
