#pragma once
#include <cstdarg>
namespace Eloquent {
    namespace ML {
        namespace Port {
            class DecisionTree {
                public:
                    
                    int predict(float *x) {
                        if (x[6] <= 148.13999938964844) {
                            return 3;
                        }

                        else {
                            if (x[3] <= 0.24499999731779099) {
                                if (x[7] <= 59.8700008392334) {
                                    if (x[2] <= 0.32500000298023224) {
                                        if (x[3] <= 0.23499999940395355) {
                                            if (x[2] <= 0.3050000071525574) {
                                                return 0;
                                            }

                                            else {
                                                return 0;
                                            }
                                        }

                                        else {
                                            if (x[5] <= 0.13499999791383743) {
                                                return 0;
                                            }

                                            else {
                                                return 1;
                                            }
                                        }
                                    }

                                    else {
                                        if (x[7] <= 41.90999984741211) {
                                            if (x[7] <= 39.625) {
                                                return 4;
                                            }

                                            else {
                                                return 0;
                                            }
                                        }

                                        else {
                                            if (x[3] <= 0.23499999940395355) {
                                                return 4;
                                            }

                                            else {
                                                return 4;
                                            }
                                        }
                                    }
                                }

                                else {
                                    return 2;
                                }
                            }

                            else {
                                if (x[2] <= 0.29500000178813934) {
                                    if (x[5] <= 0.13499999791383743) {
                                        if (x[4] <= 0.2149999961256981) {
                                            if (x[7] <= 38.375) {
                                                return 0;
                                            }

                                            else {
                                                return 1;
                                            }
                                        }

                                        else {
                                            if (x[2] <= 0.24499999731779099) {
                                                return 1;
                                            }

                                            else {
                                                return 0;
                                            }
                                        }
                                    }

                                    else {
                                        if (x[6] <= 1219.0) {
                                            return 0;
                                        }

                                        else {
                                            return 1;
                                        }
                                    }
                                }

                                else {
                                    if (x[7] <= 41.53000068664551) {
                                        if (x[1] <= 8.96500015258789) {
                                            return 4;
                                        }

                                        else {
                                            return 0;
                                        }
                                    }

                                    else {
                                        return 4;
                                    }
                                }
                            }
                        }
                    }

                protected:
                };
            }
        }
    }
